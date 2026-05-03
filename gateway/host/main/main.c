#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "hal_actuator.h"
#include "network_manager.h"
#include "rule_engine.h"
#include "zigbee_coordinator.h"
#include "provisioning.h"
#include "ota_manager.h"

static const char *TAG = "GW_MAIN";

#define GW_HEARTBEAT_INTERVAL_MS  10000
#define GW_FAULT_REBOOT_DELAY_MS  30000
#define GW_MAX_FAULT_RETRIES      3

typedef enum {
    GW_STATE_BOOT,
    GW_STATE_PROVISIONING,
    GW_STATE_CONNECTING,
    GW_STATE_OPERATIONAL,
    GW_STATE_OTA_UPDATE,
    GW_STATE_FAULT,
} gw_state_t;

static gw_state_t s_state = GW_STATE_BOOT;
static int s_fault_retries = 0;
static EventGroupHandle_t s_gw_events = NULL;
#define GW_ZB_READY_BIT    BIT0
#define GW_NET_READY_BIT   BIT1
#define GW_OTA_REQUEST_BIT BIT2

static void on_node_data_received(const zigbee_coord_node_data_t *data)
{
    if (!data) {
        return;
    }

    ESP_LOGI(TAG, "Data from Node 0x%04x EP%d", data->short_addr, data->src_endpoint);

    if (!isnan(data->temperature) || !isnan(data->humidity)) {
        node_telemetry_t telemetry = {
            .temperature = data->temperature,
            .humidity    = data->humidity,
        };
        rule_engine_evaluate((uint32_t)data->short_addr, &telemetry);
    }

    char topic[80];
    char payload[128];
    snprintf(topic, sizeof(topic),
             "agri/gw/gw_001/node/0x%04x/telemetry", data->short_addr);

    if (!isnan(data->temperature)) {
        snprintf(payload, sizeof(payload),
                 "{\"type\":\"temp\",\"value\":%.2f,\"unit\":\"C\"}",
                 data->temperature);
        network_manager_publish_telemetry(topic, payload);
    }

    if (!isnan(data->humidity)) {
        snprintf(payload, sizeof(payload),
                 "{\"type\":\"hum\",\"value\":%.2f,\"unit\":\"%%\"}",
                 data->humidity);
        network_manager_publish_telemetry(topic, payload);
    }
}

static void transition_to(gw_state_t new_state)
{
    ESP_LOGI(TAG, "State transition: %d -> %d", s_state, new_state);
    s_state = new_state;
}

static esp_err_t enter_fault_state(const char *reason)
{
    ESP_LOGE(TAG, "Entering FAULT state: %s", reason);
    s_fault_retries++;

    if (s_fault_retries > GW_MAX_FAULT_RETRIES) {
        ESP_LOGE(TAG, "Max fault retries exceeded, halting");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(60000));
        }
    }

    nvs_handle_t nvs = {0};
    if (nvs_open("gw_state", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_str(nvs, "last_fault", reason);
        nvs_set_i32(nvs, "fault_count", s_fault_retries);
        nvs_commit(nvs);
        nvs_close(nvs);
    }

    transition_to(GW_STATE_FAULT);
    ESP_LOGI(TAG, "Rebooting in %d ms (retry %d/%d)",
             GW_FAULT_REBOOT_DELAY_MS, s_fault_retries, GW_MAX_FAULT_RETRIES);
    vTaskDelay(pdMS_TO_TICKS(GW_FAULT_REBOOT_DELAY_MS));
    esp_restart();
    return ESP_OK;
}

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " AgriNetPro Gateway - Starting");
    ESP_LOGI(TAG, "========================================");

    s_gw_events = xEventGroupCreate();
    if (!s_gw_events) {
        enter_fault_state("Failed to create event group");
    }

    transition_to(GW_STATE_BOOT);

    /* ---- BOOT State ---- */
    ESP_LOGI(TAG, "[BOOT] Initializing NVS...");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        enter_fault_state("NVS init failed");
    }

    ret = nvs_flash_init_partition("zb_storage");
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase_partition("zb_storage"));
        ret = nvs_flash_init_partition("zb_storage");
    }
    if (ret != ESP_OK) {
        enter_fault_state("Zigbee NVS init failed");
    }

    ESP_LOGI(TAG, "[BOOT] Initializing hardware actuators...");
    ret = hal_actuator_init();
    if (ret != ESP_OK) {
        enter_fault_state("Actuator init failed");
    }

    /* Check if provisioning is needed */
    if (!provisioning_is_done()) {
        transition_to(GW_STATE_PROVISIONING);
        ESP_LOGW(TAG, "[PROVISIONING] No WiFi credentials found");
        ret = provisioning_start();
        if (ret != ESP_OK) {
            enter_fault_state("Provisioning start failed");
        }

        ESP_LOGI(TAG, "[PROVISIONING] Waiting for provisioning to complete...");
        while (!provisioning_is_done()) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        ESP_LOGI(TAG, "[PROVISIONING] Credentials stored, proceeding to connect");
    }

    /* ---- CONNECTING State ---- */
    transition_to(GW_STATE_CONNECTING);
    ESP_LOGI(TAG, "[CONNECTING] Initializing network (WiFi + MQTT)...");
    ret = network_manager_init();
    if (ret != ESP_OK) {
        enter_fault_state("Network init failed");
    }

    ESP_LOGI(TAG, "[CONNECTING] Initializing Zigbee Coordinator...");
    ret = zigbee_coordinator_init(on_node_data_received);
    if (ret != ESP_OK) {
        enter_fault_state("Zigbee coordinator init failed");
    }

    /* Mark OTA partition as valid now that boot succeeded */
    ret = ota_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "[CONNECTING] OTA manager init failed (non-fatal)");
    } else {
        ota_manager_mark_valid();
    }

    xEventGroupSetBits(s_gw_events, GW_ZB_READY_BIT | GW_NET_READY_BIT);

    /* ---- OPERATIONAL State ---- */
    transition_to(GW_STATE_OPERATIONAL);
    ESP_LOGI(TAG, "[OPERATIONAL] Gateway ready. Waiting for node data...");

    s_fault_retries = 0;

    while (1) {
        EventBits_t bits = xEventGroupWaitBits(
            s_gw_events,
            GW_ZB_READY_BIT | GW_NET_READY_BIT | GW_OTA_REQUEST_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(GW_HEARTBEAT_INTERVAL_MS));

        if (bits & GW_OTA_REQUEST_BIT) {
            transition_to(GW_STATE_OTA_UPDATE);
            ESP_LOGI(TAG, "[OTA_UPDATE] Starting OTA update");
            /* OTA URL would come from MQTT command or NVS */
            const char *fw_url = "https://ota.agrinet.local/firmware/latest.bin";
            ret = ota_manager_start(fw_url);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "[OTA_UPDATE] Failed to start OTA");
                transition_to(GW_STATE_OPERATIONAL);
            }
        }

        if (bits & GW_ZB_READY_BIT) {
            ESP_LOGI(TAG, "[OPERATIONAL] Gateway heartbeat OK (Zigbee active)");
        } else {
            ESP_LOGW(TAG, "[OPERATIONAL] Heartbeat timeout, checking subsystems...");
        }

        if (s_state != GW_STATE_OPERATIONAL) {
            break;
        }
    }
}
