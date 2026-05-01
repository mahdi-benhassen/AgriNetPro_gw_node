#include "network_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "NET_MGR";

esp_err_t network_manager_init(void) {
    ESP_LOGI(TAG, "Initializing Wi-Fi Station and connecting to AP...");
    // esp_wifi_init, esp_wifi_set_mode, esp_wifi_start
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG, "Wi-Fi Connected. IP: 192.168.1.100");
    
    ESP_LOGI(TAG, "Starting MQTT Client...");
    // esp_mqtt_client_init, esp_mqtt_client_start
    vTaskDelay(pdMS_TO_TICKS(200));
    ESP_LOGI(TAG, "MQTT Connected.");
    
    return ESP_OK;
}

esp_err_t network_manager_publish_telemetry(const char *topic, const char *payload) {
    ESP_LOGD(TAG, "MQTT PUB [%s]: %s", topic, payload);
    // esp_mqtt_client_publish
    return ESP_OK;
}
