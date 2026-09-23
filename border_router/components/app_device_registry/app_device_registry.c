/**
 * @file app_device_registry.c
 */

#include "app_device_registry.h"
#include "app_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <time.h>

static const char *TAG = TAG_DEV_MGR;

static app_device_entry_t        s_devices[DEVICE_REGISTRY_MAX_NODES];
static int64_t                   s_last_seen_mono[DEVICE_REGISTRY_MAX_NODES];
static int                       s_count          = 0;
static SemaphoreHandle_t         s_mutex          = NULL;
static app_device_telemetry_cb_t s_telem_cb       = NULL;
static app_device_status_cb_t    s_status_cb      = NULL;
static app_device_cmd_ack_cb_t   s_cmd_ack_cb     = NULL;
static bool                      s_dirty          = false;
static uint8_t                   s_cmd_id_counter = 0;

/* ─── Internal helpers ────────────────────────────────────────────────────── */

static app_device_entry_t *find_by_eui64(uint64_t eui64)
{
    for (int i = 0; i < s_count; i++) {
        if (s_devices[i].eui64 == eui64) return &s_devices[i];
    }
    return NULL;
}

static app_device_entry_t *alloc_entry(uint64_t eui64)
{
    if (s_count >= DEVICE_REGISTRY_MAX_NODES) return NULL;
    app_device_entry_t *e = &s_devices[s_count++];
    memset(e, 0, sizeof(*e));
    e->eui64 = eui64;
    return e;
}

/* ─── Public API ──────────────────────────────────────────────────────────── */

esp_err_t app_device_registry_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) return ESP_ERR_NO_MEM;
    memset(s_devices, 0, sizeof(s_devices));
    s_count = 0;
    app_device_registry_load_from_nvs();
    ESP_LOGI(TAG, "Device registry initialised (active %d, max %d nodes)",
             s_count, DEVICE_REGISTRY_MAX_NODES);
    return ESP_OK;
}

esp_err_t app_device_registry_register(const app_node_reg_payload_t *reg,
                                        const char *ipv6_str)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    app_device_entry_t *e = find_by_eui64(reg->eui64);
    if (!e) {
        e = alloc_entry(reg->eui64);
        if (!e) {
            xSemaphoreGive(s_mutex);
            ESP_LOGE(TAG, "Registry full! Cannot register %016llX",
                     (unsigned long long)reg->eui64);
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "New node registered: %016llX label='%s'",
                 (unsigned long long)reg->eui64, reg->label);
    } else {
        ESP_LOGI(TAG, "Node re-registered: %016llX", (unsigned long long)reg->eui64);
    }

    strncpy(e->label, reg->label, sizeof(e->label) - 1);
    if (ipv6_str) strncpy(e->ipv6_addr, ipv6_str, sizeof(e->ipv6_addr) - 1);
    e->sensor_type       = reg->sensor_type;
    e->fw_major          = reg->fw_major;
    e->fw_minor          = reg->fw_minor;
    e->report_interval_s = reg->report_interval_s;
    e->online            = true;
    e->last_seen         = time(NULL);
    s_last_seen_mono[e - s_devices] = esp_timer_get_time();

    xSemaphoreGive(s_mutex);

    app_device_registry_save_to_nvs();
    return ESP_OK;
}

esp_err_t app_device_registry_update(const app_sensor_payload_t *payload,
                                      const char *ipv6_str)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    app_device_entry_t *e = find_by_eui64(payload->eui64);
    if (!e) {
        /* Auto-register with minimal info */
        e = alloc_entry(payload->eui64);
        if (!e) {
            xSemaphoreGive(s_mutex);
            return ESP_ERR_NO_MEM;
        }
        snprintf(e->label, sizeof(e->label), "node_%08lX",
                 (unsigned long)(payload->eui64 & 0xFFFFFFFFUL));
        ESP_LOGI(TAG, "Auto-registered node %016llX",
                 (unsigned long long)payload->eui64);
    }

    if (ipv6_str) strncpy(e->ipv6_addr, ipv6_str, sizeof(e->ipv6_addr) - 1);
    e->online         = true;
    e->last_seen      = time(NULL);
    s_last_seen_mono[e - s_devices] = esp_timer_get_time();
    e->last_reading   = *payload;
    e->total_reports++;
    s_dirty           = true;

    ESP_LOGI(TAG, "Node %016llX → T=%.1f°C RH=%.1f%% (reports=%"PRIu32")",
             (unsigned long long)payload->eui64,
             TEMP_RAW_TO_FLOAT(payload->temperature_c),
             HUM_RAW_TO_FLOAT(payload->humidity_pct),
             e->total_reports);

    /* Snapshot for callback (call outside lock) */
    app_device_entry_t snap = *e;
    xSemaphoreGive(s_mutex);

    if (s_telem_cb) {
        s_telem_cb(&snap, payload);
    }
    return ESP_OK;
}

esp_err_t app_device_registry_get(uint64_t eui64, app_device_entry_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    app_device_entry_t *e = find_by_eui64(eui64);
    if (e) *out = *e;
    xSemaphoreGive(s_mutex);
    return e ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t app_device_registry_queue_cmd(uint64_t eui64,
                                         app_cmd_type_t cmd_type,
                                         uint16_t param,
                                         const char *payload)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    app_device_entry_t *e = find_by_eui64(eui64);
    if (!e) {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    e->pending_cmd_type    = cmd_type;
    e->pending_cmd_id      = ++s_cmd_id_counter;
    e->pending_cmd_param   = param;
    if (payload) {
        strncpy(e->pending_cmd_payload, payload,
                sizeof(e->pending_cmd_payload) - 1);
    } else {
        memset(e->pending_cmd_payload, 0, sizeof(e->pending_cmd_payload));
    }
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "Queued cmd type=%d id=%d for node %016llX",
             cmd_type, s_cmd_id_counter, (unsigned long long)eui64);
    return ESP_OK;
}

esp_err_t app_device_registry_pop_cmd(uint64_t eui64, app_cmd_payload_t *cmd)
{
    memset(cmd, 0, sizeof(*cmd));
    cmd->cmd_type = CMD_NONE;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    app_device_entry_t *e = find_by_eui64(eui64);
    if (e && e->pending_cmd_type != CMD_NONE) {
        cmd->cmd_type  = e->pending_cmd_type;
        cmd->cmd_id    = e->pending_cmd_id;
        cmd->cmd_param = e->pending_cmd_param;
        memcpy(cmd->cmd_payload, e->pending_cmd_payload,
               sizeof(cmd->cmd_payload));
        /* Clear after pop */
        e->pending_cmd_type = CMD_NONE;
        e->pending_cmd_id   = 0;
    }
    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

void app_device_registry_foreach(void (*cb)(const app_device_entry_t *dev,
                                             void *user_data),
                                  void *user_data)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < s_count; i++) {
        cb(&s_devices[i], user_data);
    }
    xSemaphoreGive(s_mutex);
}

int app_device_registry_count(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    int n = s_count;
    xSemaphoreGive(s_mutex);
    return n;
}

esp_err_t app_device_registry_get_by_ipv6(const char *ipv6_str, app_device_entry_t *out)
{
    if (!ipv6_str || !out) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < s_count; i++) {
        if (s_devices[i].ipv6_addr[0] != '\0' && strcmp(s_devices[i].ipv6_addr, ipv6_str) == 0) {
            *out = s_devices[i];
            xSemaphoreGive(s_mutex);
            return ESP_OK;
        }
    }
    xSemaphoreGive(s_mutex);
    return ESP_ERR_NOT_FOUND;
}

void app_device_registry_set_telemetry_cb(app_device_telemetry_cb_t cb)
{
    s_telem_cb = cb;
}

void app_device_registry_set_status_cb(app_device_status_cb_t cb)
{
    s_status_cb = cb;
}

void app_device_registry_sweep_offline(void)
{
    int64_t now_us = esp_timer_get_time();
    app_device_entry_t offline_list[DEVICE_REGISTRY_MAX_NODES];
    int offline_count = 0;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < s_count; i++) {
        if (s_devices[i].online &&
            (now_us - s_last_seen_mono[i]) > ((int64_t)DEVICE_ONLINE_TTL_S * 1000000LL)) {
            s_devices[i].online = false;
            offline_list[offline_count++] = s_devices[i];
            ESP_LOGW(TAG, "Node %016llX went offline (no report for >%ds)",
                     (unsigned long long)s_devices[i].eui64,
                     DEVICE_ONLINE_TTL_S);
        }
    }
    xSemaphoreGive(s_mutex);

    if (s_status_cb) {
        for (int i = 0; i < offline_count; i++) {
            s_status_cb(&offline_list[i], false);
        }
    }

    if (s_dirty) {
        app_device_registry_save_to_nvs();
    }
}

esp_err_t app_device_registry_save_to_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_REGISTRY, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace %s: %s",
                 NVS_NAMESPACE_REGISTRY, esp_err_to_name(err));
        return err;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    uint32_t count = (uint32_t)s_count;
    err = nvs_set_u32(handle, NVS_KEY_REGISTRY_COUNT, count);
    if (err == ESP_OK && count > 0) {
        err = nvs_set_blob(handle, NVS_KEY_REGISTRY_BLOB, s_devices, count * sizeof(app_device_entry_t));
    }
    xSemaphoreGive(s_mutex);

    if (err == ESP_OK) {
        err = nvs_commit(handle);
        s_dirty = false;
        ESP_LOGD(TAG, "Persisted %"PRIu32" devices to NVS", count);
    } else {
        ESP_LOGE(TAG, "Failed writing devices to NVS: %s", esp_err_to_name(err));
    }
    nvs_close(handle);
    return err;
}

esp_err_t app_device_registry_load_from_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_REGISTRY, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No NVS registry found (initial boot)");
        return err;
    }

    uint32_t count = 0;
    err = nvs_get_u32(handle, NVS_KEY_REGISTRY_COUNT, &count);
    if (err != ESP_OK || count == 0) {
        nvs_close(handle);
        return ESP_OK;
    }

    if (count > DEVICE_REGISTRY_MAX_NODES) {
        count = DEVICE_REGISTRY_MAX_NODES;
    }

    size_t blob_len = count * sizeof(app_device_entry_t);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    err = nvs_get_blob(handle, NVS_KEY_REGISTRY_BLOB, s_devices, &blob_len);
    if (err == ESP_OK) {
        s_count = (int)(blob_len / sizeof(app_device_entry_t));
        for (int i = 0; i < s_count; i++) {
            s_devices[i].online = false; // Mark offline until telemetry arrives
            s_last_seen_mono[i] = 0;
        }
        ESP_LOGI(TAG, "Restored %d devices from NVS flash", s_count);
    } else {
        ESP_LOGW(TAG, "Failed reading device blob from NVS: %s", esp_err_to_name(err));
    }
    xSemaphoreGive(s_mutex);

    nvs_close(handle);
    return err;
}

void app_device_registry_set_cmd_ack_cb(app_device_cmd_ack_cb_t cb)
{
    s_cmd_ack_cb = cb;
}

esp_err_t app_device_registry_record_cmd_ack(const app_cmd_ack_payload_t *ack)
{
    if (!ack) return ESP_ERR_INVALID_ARG;

    app_device_entry_t dev_copy;
    bool found = false;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    app_device_entry_t *e = find_by_eui64(ack->eui64);
    if (e) {
        e->last_ack_cmd_id   = ack->cmd_id;
        e->last_ack_cmd_type = ack->cmd_type;
        e->last_ack_status   = ack->status_code;
        strncpy(e->last_ack_msg, ack->message, sizeof(e->last_ack_msg) - 1);
        e->last_ack_msg[sizeof(e->last_ack_msg) - 1] = '\0';
        e->last_ack_time     = time(NULL);
        dev_copy = *e;
        found = true;
        s_dirty = true;
    }
    xSemaphoreGive(s_mutex);

    if (!found) {
        ESP_LOGW(TAG, "Command ACK from unregistered node %016llX",
                 (unsigned long long)ack->eui64);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Node %016llX ACK: cmd_id=%d status=%d msg='%s'",
             (unsigned long long)ack->eui64, ack->cmd_id, ack->status_code, ack->message);

    if (s_cmd_ack_cb) {
        s_cmd_ack_cb(&dev_copy, ack);
    }
    return ESP_OK;
}
