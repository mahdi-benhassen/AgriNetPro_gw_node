#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "ZB_RCP";

void app_main(void)
{
    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Starting Zigbee RCP for ESP32-H2");

    /* Initialize Zigbee RCP */
    // Note: The actual RCP firmware is usually the ot_rcp example from esp-idf/openthread.
    // esp_zb_rcp_init();

    ESP_LOGI(TAG, "Zigbee RCP is running and waiting for Host (S3) commands via UART");
}
