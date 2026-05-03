#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "hal_sensor.h"
#include "power_manager.h"
#include "zigbee_node.h"

static const char *TAG = "NODE_MAIN";

#define NODE_DEMO_SLEEP_SEC  15

static EventGroupHandle_t s_app_event_group = NULL;
#define APP_SLEEP_READY_BIT BIT0

static void on_sleep_ready(void)
{
    if (s_app_event_group) {
        xEventGroupSetBits(s_app_event_group, APP_SLEEP_READY_BIT);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " AgriNetPro IoT Node - Wake Up");
    ESP_LOGI(TAG, "========================================");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = nvs_flash_init_partition("zb_storage");
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase_partition("zb_storage"));
        ret = nvs_flash_init_partition("zb_storage");
    }
    ESP_ERROR_CHECK(ret);

    s_app_event_group = xEventGroupCreate();

    /* ---- 1. Initialize Sensors ---- */
    ESP_ERROR_CHECK(hal_sensor_init());

    float temp       = 0.0f;
    float humidity   = 0.0f;
    uint32_t batt_mv = 0;

    /* ---- 2. Acquire Sensor Data ---- */
    hal_sensor_read_temperature(&temp);
    hal_sensor_read_humidity(&humidity);
    power_manager_read_battery(&batt_mv);

    ESP_LOGI(TAG, "Sensor Data -> Temp: %.1f C, Hum: %.1f %%, Batt: %lu mV",
             temp, humidity, batt_mv);

    /* ---- 3. Initialize Zigbee & Join Network ---- */
    ESP_ERROR_CHECK(zigbee_node_init(on_sleep_ready));

    ESP_LOGI(TAG, "Waiting to join Zigbee network...");
    for (int i = 0; i < ZIGBEE_NODE_JOIN_TIMEOUT_SEC; i++) {
        if (zigbee_node_is_connected()) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (!zigbee_node_is_connected()) {
        ESP_LOGW(TAG, "Failed to join network within timeout. Sleeping...");
        goto sleep;
    }

    /* ---- 4. Transmit Data via Zigbee ZCL Reports ---- */
    ESP_LOGI(TAG, "Transmitting sensor data via Zigbee...");
    zigbee_node_report_temperature(temp);
    zigbee_node_report_humidity(humidity);
    zigbee_node_report_battery(batt_mv);

    zigbee_node_signal_tx_done();

    ESP_LOGI(TAG, "Waiting for Zigbee transmission to complete...");
    xEventGroupWaitBits(s_app_event_group, APP_SLEEP_READY_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(5000));
    ESP_LOGI(TAG, "Zigbee transmission complete.");

sleep:
    /* ---- 5. Shutdown Peripherals ---- */
    hal_sensor_deinit();

    /* ---- 6. Calculate dynamic sleep interval based on battery ---- */
    uint32_t sleep_duration = power_manager_get_sleep_duration(batt_mv);
    if (sleep_duration > NODE_DEMO_SLEEP_SEC) {
        sleep_duration = NODE_DEMO_SLEEP_SEC;
    }

    /* ---- 7. Enter Deep Sleep ---- */
    ESP_ERROR_CHECK(power_manager_configure_sleep(sleep_duration, true));
    ESP_LOGI(TAG, "Entering deep sleep for %lu seconds...", sleep_duration);
    power_manager_enter_deep_sleep();
}
