#include "rule_engine.h"
#include "hal_actuator.h"
#include "esp_log.h"

static const char *TAG = "RULE_ENG";

void rule_engine_evaluate(uint32_t node_id, const node_telemetry_t *telemetry) {
    if (!telemetry) return;

    ESP_LOGI(TAG, "Evaluating rules for Node %lu", node_id);

    // Rule 1: High Temperature -> Turn on Fan
    if (telemetry->temperature > 30.0f) {
        ESP_LOGW(TAG, "Rule Triggered: High Temp (%.1f > 30.0). Activating Fan.", telemetry->temperature);
        hal_actuator_set(ACTUATOR_FAN_1, true);
    } else if (telemetry->temperature < 25.0f) {
        hal_actuator_set(ACTUATOR_FAN_1, false);
    }

    // Rule 2: Low Humidity -> Open Water Valve
    if (telemetry->humidity < 50.0f) {
        ESP_LOGW(TAG, "Rule Triggered: Low Humidity (%.1f < 50.0). Opening Valve.", telemetry->humidity);
        hal_actuator_set(ACTUATOR_WATER_VALVE_1, true);
    } else if (telemetry->humidity > 60.0f) {
        hal_actuator_set(ACTUATOR_WATER_VALVE_1, false);
    }
}
