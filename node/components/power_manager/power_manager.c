#include "power_manager.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_random.h"

static const char *TAG = "POWER_MGR";

void power_manager_configure_sleep(uint32_t sleep_duration_sec) {
    ESP_LOGI(TAG, "Configuring timer wakeup for %lu seconds", sleep_duration_sec);
    esp_sleep_enable_timer_wakeup((uint64_t)sleep_duration_sec * 1000000ULL);
}

void power_manager_enter_deep_sleep(void) {
    ESP_LOGI(TAG, "Entering deep sleep...");
    // Wait for UART to flush to ensure logs are printed
    esp_deep_sleep_start();
}

esp_err_t power_manager_read_battery(uint32_t *voltage_mv) {
    if (voltage_mv == NULL) return ESP_ERR_INVALID_ARG;
    
    // Simulate ADC reading for a 3.7V LiPo (3.2V to 4.2V)
    *voltage_mv = 3200 + (esp_random() % 1000);
    ESP_LOGD(TAG, "Read Battery: %lu mV", *voltage_mv);
    
    return ESP_OK;
}
