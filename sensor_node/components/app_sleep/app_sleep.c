/**
 * @file app_sleep.c
 */

#include "app_sleep.h"
#include "app_config.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_openthread.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <openthread/thread.h>
#include <openthread/link.h>
#include <string.h>

static const char *TAG = TAG_SLEEP;

#define NVS_NAMESPACE   "thread_sleep"
#define NVS_KEY_DATASET "dataset"

static app_sleep_mode_t s_mode = SLEEP_MODE_LIGHT;

/* ─── Sleepy End Device configuration ─────────────────────────────────────── */
static void configure_sed(void)
{
    otInstance *ot = esp_openthread_get_instance();

    /* Set device role to Sleepy End Device */
    otLinkModeConfig mode = {
        .mRxOnWhenIdle   = false,   /* SED: radio OFF between polls         */
        .mDeviceType     = false,   /* MTD (Minimal Thread Device)          */
        .mNetworkData    = false,   /* Stable network data only             */
    };
    otThreadSetLinkMode(ot, mode);

    /* Poll period for SED (how often to check parent for buffered data) */
    otLinkSetPollPeriod(ot, SENSOR_REPORT_INTERVAL_S * 1000UL);

    ESP_LOGI(TAG, "Thread configured as Sleepy End Device, poll=%us",
             SENSOR_REPORT_INTERVAL_S);
}

/* ─── Public API ──────────────────────────────────────────────────────────── */

esp_err_t app_sleep_init(app_sleep_mode_t mode)
{
    s_mode = mode;

    if (mode == SLEEP_MODE_LIGHT) {
        /* Enable automatic light-sleep with Thread modem-sleep */
        esp_sleep_enable_timer_wakeup(0);   /* cleared – task delay drives it */
    }

    configure_sed();
    ESP_LOGI(TAG, "Sleep mode: %s",
             mode == SLEEP_MODE_LIGHT ? "LIGHT (modem-sleep)" : "DEEP");
    return ESP_OK;
}

bool app_sleep_is_wakeup_from_deep(void)
{
    return (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER);
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
    otInstance *ot = esp_openthread_get_instance();
    otOperationalDatasetTlvs dataset_tlvs;

    if (otDatasetGetActiveTlvs(ot, &dataset_tlvs) == OT_ERROR_NONE) {
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
