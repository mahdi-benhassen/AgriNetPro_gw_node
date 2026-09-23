/**
 * @file app_sleep.c
 */

#include "app_sleep.h"
#include "app_config.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <openthread/thread.h>
#include <openthread/link.h>
#include <openthread/dataset.h>
#include <string.h>

static const char *TAG = TAG_SLEEP;

#define NVS_NAMESPACE   "thread_sleep"
#define NVS_KEY_DATASET "dataset"

static app_sleep_mode_t s_mode = SLEEP_MODE_LIGHT;

/* ─── Sleepy End Device configuration ─────────────────────────────────────── */
esp_err_t app_sleep_configure_sed(void)
{
    esp_openthread_lock_acquire(portMAX_DELAY);
    otInstance *ot = esp_openthread_get_instance();
    if (!ot) {
        esp_openthread_lock_release();
        ESP_LOGE(TAG, "OpenThread instance is NULL, cannot configure SED");
        return ESP_ERR_INVALID_STATE;
    }

    /* Set device role to Sleepy End Device */
    otLinkModeConfig mode = {
        .mRxOnWhenIdle   = false,   /* SED: radio OFF between polls         */
        .mDeviceType     = false,   /* MTD (Minimal Thread Device)          */
        .mNetworkData    = false,   /* Stable network data only             */
    };
    otThreadSetLinkMode(ot, mode);

    /* Poll period for SED (how often to check parent for buffered data) */
    otLinkSetPollPeriod(ot, SENSOR_REPORT_INTERVAL_S * 1000UL);
    esp_openthread_lock_release();

    ESP_LOGI(TAG, "Thread configured as Sleepy End Device, poll=%us",
             SENSOR_REPORT_INTERVAL_S);
    return ESP_OK;
}

/* ─── Public API ──────────────────────────────────────────────────────────── */

esp_err_t app_sleep_init(app_sleep_mode_t mode)
{
    s_mode = mode;

    if (mode == SLEEP_MODE_LIGHT) {
        /* Enable automatic light-sleep with Thread modem-sleep */
        esp_sleep_enable_timer_wakeup(0);   /* cleared – task delay drives it */
    }

    ESP_LOGI(TAG, "Sleep mode initialized: %s",
             mode == SLEEP_MODE_LIGHT ? "LIGHT (modem-sleep)" : "DEEP");
    return ESP_OK;
}

bool app_sleep_is_wakeup_from_deep(void)
{
    return (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER);
}

esp_err_t app_sleep_restore_dataset(void)
{
    nvs_handle_t nvs;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "No cached Thread dataset in NVS (%s)", esp_err_to_name(ret));
        return ret;
    }

    otOperationalDatasetTlvs dataset_tlvs;
    size_t len = sizeof(dataset_tlvs.mTlvs);
    ret = nvs_get_blob(nvs, NVS_KEY_DATASET, dataset_tlvs.mTlvs, &len);
    nvs_close(nvs);

    if (ret != ESP_OK || len == 0) {
        ESP_LOGW(TAG, "Failed to read Thread dataset blob from NVS: %s", esp_err_to_name(ret));
        return ret != ESP_OK ? ret : ESP_ERR_NOT_FOUND;
    }
    dataset_tlvs.mLength = (uint8_t)len;

    esp_openthread_lock_acquire(portMAX_DELAY);
    otInstance *ot = esp_openthread_get_instance();
    if (!ot) {
        esp_openthread_lock_release();
        return ESP_ERR_INVALID_STATE;
    }

    otError ot_err = otDatasetSetActiveTlvs(ot, &dataset_tlvs);
    esp_openthread_lock_release();

    if (ot_err != OT_ERROR_NONE) {
        ESP_LOGE(TAG, "otDatasetSetActiveTlvs failed: %d", (int)ot_err);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Restored cached Thread dataset (%u bytes)", (unsigned int)len);
    return ESP_OK;
}

void app_sleep_enter(uint32_t duration_ms)
{
    if (s_mode == SLEEP_MODE_LIGHT) {
        /* Simple FreeRTOS delay — Thread stack handles modem-sleep internally */
        ESP_LOGI(TAG, "Light-sleep for %"PRIu32" ms", duration_ms);
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
        return;
    }

    /* Deep-sleep: persist Thread operational dataset so we can re-join fast */
    esp_openthread_lock_acquire(portMAX_DELAY);
    otInstance *ot = esp_openthread_get_instance();
    otOperationalDatasetTlvs dataset_tlvs;
    bool has_dataset = false;

    if (ot && otDatasetGetActiveTlvs(ot, &dataset_tlvs) == OT_ERROR_NONE) {
        has_dataset = true;
    }
    esp_openthread_lock_release();

    if (has_dataset) {
        nvs_handle_t nvs;
        if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
            nvs_set_blob(nvs, NVS_KEY_DATASET,
                         dataset_tlvs.mTlvs, dataset_tlvs.mLength);
            nvs_commit(nvs);
            nvs_close(nvs);
            ESP_LOGI(TAG, "Thread dataset saved (%d bytes)", dataset_tlvs.mLength);
        }
    }

    ESP_LOGI(TAG, "Entering deep-sleep for %"PRIu32" ms", duration_ms);
    esp_sleep_enable_timer_wakeup((uint64_t)duration_ms * 1000ULL);
    esp_deep_sleep_start();
    /* DOES NOT RETURN */
}
