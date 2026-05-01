#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "hal_actuator.h"
#include "network_manager.h"
#include "rule_engine.h"
#include "zigbee_coordinator.h"

static const char *TAG = "GW_MAIN";

/**
 * @brief Callback invoked by the Zigbee Coordinator when a node sends
 *        an attribute report. This bridges the Zigbee layer to the
 *        application layer (rule engine + MQTT).
 */
static void on_node_data_received(const zigbee_coord_node_data_t *data)
{
    if (!data) {
        return;
    }

    ESP_LOGI(TAG, "Data from Node 0x%04x EP%d", data->short_addr, data->src_endpoint);

    /* Feed valid readings into the rule engine */
    if (!isnan(data->temperature) || !isnan(data->humidity)) {
        node_telemetry_t telemetry = {
            .temperature = data->temperature,
            .humidity    = data->humidity,
        };
        rule_engine_evaluate((uint32_t)data->short_addr, &telemetry);
    }

    /* Publish to cloud via MQTT */
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

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " AgriNetPro Gateway - Starting");
    ESP_LOGI(TAG, "========================================");

    /* ---- NVS Init ---- */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* ---- 1. Initialize Hardware Actuators ---- */
    ESP_ERROR_CHECK(hal_actuator_init());

    /* ---- 2. Initialize Network (Wi-Fi + MQTT) ---- */
    ESP_ERROR_CHECK(network_manager_init());

    /* ---- 3. Initialize Zigbee Coordinator ---- */
    ESP_ERROR_CHECK(zigbee_coordinator_init(on_node_data_received));

    ESP_LOGI(TAG, "Gateway initialized. Waiting for node data...");

    /* Main task can handle background diagnostics */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "Gateway heartbeat OK");
    }
}
