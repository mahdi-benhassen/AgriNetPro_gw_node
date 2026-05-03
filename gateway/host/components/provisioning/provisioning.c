#include "provisioning.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"
#include "wifi_provisioning/scheme_softap.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "PROVISION";

#define PROV_NVS_NAMESPACE      "wifi_creds"
#define PROV_NVS_KEY_SSID       "ssid"
#define PROV_NVS_KEY_PASS       "pass"
#define PROV_MAX_CRED_LEN       64
#define PROV_SERVICE_NAME       "AgriNetPro_GW"

static bool s_provisioning_done = false;

static void prov_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
        case WIFI_PROV_START:
            ESP_LOGI(TAG, "Provisioning started");
            break;
        case WIFI_PROV_CRED_RECV: {
            wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
            ESP_LOGI(TAG, "Received credentials for SSID: %s", (const char *)wifi_sta_cfg->ssid);
            break;
        }
        case WIFI_PROV_CRED_FAIL:
            ESP_LOGE(TAG, "Failed to connect with received credentials");
            break;
        case WIFI_PROV_CRED_SUCCESS:
            ESP_LOGI(TAG, "Provisioning successful");
            break;
        case WIFI_PROV_END:
            ESP_LOGI(TAG, "Provisioning completed");
            s_provisioning_done = true;
            wifi_prov_mgr_deinit();
            break;
        default:
            break;
        }
    }
}

static bool check_existing_credentials(void)
{
    nvs_handle_t nvs = {0};
    esp_err_t ret = nvs_open(PROV_NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (ret != ESP_OK) {
        return false;
    }

    char ssid[PROV_MAX_CRED_LEN] = {0};
    size_t len = PROV_MAX_CRED_LEN;
    ret = nvs_get_str(nvs, PROV_NVS_KEY_SSID, ssid, &len);
    nvs_close(nvs);

    return (ret == ESP_OK);
}

esp_err_t provisioning_start(void)
{
    if (check_existing_credentials()) {
        ESP_LOGI(TAG, "Device already provisioned, skipping");
        s_provisioning_done = true;
        return ESP_OK;
    }

    ESP_LOGI(TAG, "No credentials found, starting BLE provisioning");

    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
    };

    esp_err_t ret = wifi_prov_mgr_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init provisioning manager");
        return ret;
    }

    esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, prov_event_handler, NULL);

    ret = wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, NULL, PROV_SERVICE_NAME, NULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "BLE provisioning failed, falling back to SoftAP");
        wifi_prov_mgr_deinit();

        config.scheme = wifi_prov_scheme_softap;
        ret = wifi_prov_mgr_init(&config);
        if (ret != ESP_OK) {
            return ret;
        }

        esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, prov_event_handler, NULL);

        ret = wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, NULL, PROV_SERVICE_NAME, NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SoftAP provisioning also failed");
            wifi_prov_mgr_deinit();
            return ret;
        }
    }

    ESP_LOGI(TAG, "Provisioning active. Use the AgriNetPro app to configure Wi-Fi");
    return ESP_OK;
}

bool provisioning_is_done(void)
{
    return s_provisioning_done;
}
