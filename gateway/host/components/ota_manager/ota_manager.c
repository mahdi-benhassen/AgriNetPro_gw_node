#include "ota_manager.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "OTA_MGR";

static ota_state_t s_ota_state = OTA_IDLE;
static char s_firmware_url[256] = {0};

static void ota_task(void *arg)
{
    esp_err_t err;
    esp_https_ota_config_t ota_config = {
        .http_config = {
            .url = s_firmware_url,
        },
    };

    s_ota_state = OTA_IN_PROGRESS;
    ESP_LOGI(TAG, "Starting OTA download from %s", s_firmware_url);

    err = esp_https_ota(&ota_config);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "OTA update successful, restarting...");
        s_ota_state = OTA_SUCCESS;
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(err));
        s_ota_state = OTA_FAILED;
    }

    vTaskDelete(NULL);
}

esp_err_t ota_manager_init(void)
{
    s_ota_state = OTA_IDLE;
    ESP_LOGI(TAG, "OTA manager initialized");
    return ESP_OK;
}

esp_err_t ota_manager_start(const char *firmware_url)
{
    if (s_ota_state == OTA_IN_PROGRESS) {
        ESP_LOGW(TAG, "OTA already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    if (firmware_url == NULL) {
        ESP_LOGE(TAG, "Invalid firmware URL");
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(s_firmware_url, firmware_url, sizeof(s_firmware_url) - 1);
    s_firmware_url[sizeof(s_firmware_url) - 1] = '\0';

    BaseType_t task_err = xTaskCreate(ota_task, "ota_task", 8192, NULL, 2, NULL);
    if (task_err != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t ota_manager_mark_valid(void)
{
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Current partition marked as valid");
    } else {
        ESP_LOGE(TAG, "Failed to mark partition valid: %s", esp_err_to_name(err));
    }
    return err;
}

ota_state_t ota_manager_get_state(void)
{
    return s_ota_state;
}
