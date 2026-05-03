#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_zigbee_core.h"

static const char *TAG = "ZB_RCP";

void app_main(void)
{
    ESP_LOGI(TAG, "AgriNetPro Zigbee RCP (ESP32-H2) starting...");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_zb_platform_config_t platform_cfg = {
        .radio_config = {
            .radio_mode = ZB_RADIO_MODE_NATIVE,
        },
        .host_config = {
            .host_connection_mode = ZB_HOST_CONNECTION_MODE_UART,
        },
    };

    ESP_ERROR_CHECK(esp_zb_platform_config(&platform_cfg));

    ESP_LOGI(TAG, "RCP initialized. Waiting for Host (ESP32-S3) commands via UART...");

    esp_zb_stack_main_loop();
}
