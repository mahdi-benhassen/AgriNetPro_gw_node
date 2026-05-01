#include "hal_sensor.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "HAL_SENSOR";

esp_err_t hal_sensor_init(void) {
    ESP_LOGI(TAG, "Initializing I2C sensors (Mock)");
    // Here we would configure i2c_master_init() and sensor-specific registers
    vTaskDelay(pdMS_TO_TICKS(10)); // Simulate hardware delay
    return ESP_OK;
}

esp_err_t hal_sensor_read_temperature(float *temp_c) {
    if (temp_c == NULL) return ESP_ERR_INVALID_ARG;
    
    // Simulate reading temperature between 15.0 and 35.0 C
    uint32_t raw = esp_random() % 2000;
    *temp_c = 15.0f + (float)raw / 100.0f;
    ESP_LOGD(TAG, "Read Temp: %.2f C", *temp_c);
    
    return ESP_OK;
}

esp_err_t hal_sensor_read_humidity(float *hum_rh) {
    if (hum_rh == NULL) return ESP_ERR_INVALID_ARG;
    
    // Simulate reading humidity between 40.0% and 80.0%
    uint32_t raw = esp_random() % 4000;
    *hum_rh = 40.0f + (float)raw / 100.0f;
    ESP_LOGD(TAG, "Read Humidity: %.2f %%", *hum_rh);
    
    return ESP_OK;
}

esp_err_t hal_sensor_deinit(void) {
    ESP_LOGI(TAG, "Putting sensors to sleep (Mock)");
    // Here we would send I2C sleep commands and de-init the bus
    return ESP_OK;
}
