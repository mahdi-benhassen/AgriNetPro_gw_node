#include "hal_actuator.h"
#include "esp_log.h"

static const char *TAG = "HAL_ACTUATOR";

// Mock state array
static bool g_actuator_states[ACTUATOR_MAX] = {false};

esp_err_t hal_actuator_init(void) {
    ESP_LOGI(TAG, "Initializing Actuator GPIOs");
    // In a real scenario, configure gpio_config_t here
    return ESP_OK;
}

esp_err_t hal_actuator_set(actuator_id_t id, bool state) {
    if (id >= ACTUATOR_MAX) return ESP_ERR_INVALID_ARG;
    
    if (g_actuator_states[id] != state) {
        g_actuator_states[id] = state;
        ESP_LOGI(TAG, "Actuator %d set to %s", id, state ? "ON" : "OFF");
        // E.g., gpio_set_level(PIN, state ? 1 : 0);
    }
    return ESP_OK;
}
