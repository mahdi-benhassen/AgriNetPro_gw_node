#include "rule_engine.h"
#include "hal_actuator.h"
#include "esp_log.h"
#include "cJSON.h"
#include <math.h>
#include <string.h>

static const char *TAG = "RULE_ENG";

/* Mock JSON rules representing what would be loaded from NVS/Cloud */
static const char* s_rules_json = "[\n"
    "  {\"condition\": {\"field\": \"temperature\", \"operator\": \">\", \"value\": 30.0}, \"action\": {\"actuator\": 0, \"state\": true}},\n"
    "  {\"condition\": {\"field\": \"temperature\", \"operator\": \"<\", \"value\": 25.0}, \"action\": {\"actuator\": 0, \"state\": false}},\n"
    "  {\"condition\": {\"field\": \"humidity\", \"operator\": \"<\", \"value\": 50.0}, \"action\": {\"actuator\": 1, \"state\": true}},\n"
    "  {\"condition\": {\"field\": \"humidity\", \"operator\": \">\", \"value\": 60.0}, \"action\": {\"actuator\": 1, \"state\": false}}\n"
    "]";

void rule_engine_evaluate(uint32_t node_id, const node_telemetry_t *telemetry) {
    if (!telemetry) return;

    ESP_LOGI(TAG, "Evaluating rules for Node %lu", node_id);

    cJSON *rules = cJSON_Parse(s_rules_json);
    if (!rules) {
        ESP_LOGE(TAG, "Failed to parse rule definitions");
        return;
    }

    cJSON *rule = NULL;
    cJSON_ArrayForEach(rule, rules) {
        cJSON *condition = cJSON_GetObjectItem(rule, "condition");
        cJSON *action = cJSON_GetObjectItem(rule, "action");
        if (!condition || !action) continue;

        cJSON *field = cJSON_GetObjectItem(condition, "field");
        cJSON *op = cJSON_GetObjectItem(condition, "operator");
        cJSON *val = cJSON_GetObjectItem(condition, "value");

        if (!cJSON_IsString(field) || !cJSON_IsString(op) || !cJSON_IsNumber(val)) continue;

        float current_val = NAN;
        if (strcmp(field->valuestring, "temperature") == 0) {
            current_val = telemetry->temperature;
        } else if (strcmp(field->valuestring, "humidity") == 0) {
            current_val = telemetry->humidity;
        }

        if (isnan(current_val)) continue;

        bool triggered = false;
        if (strcmp(op->valuestring, ">") == 0) {
            triggered = current_val > val->valuedouble;
        } else if (strcmp(op->valuestring, "<") == 0) {
            triggered = current_val < val->valuedouble;
        }

        if (triggered) {
            cJSON *act_id = cJSON_GetObjectItem(action, "actuator");
            cJSON *act_state = cJSON_GetObjectItem(action, "state");
            if (cJSON_IsNumber(act_id) && (cJSON_IsBool(act_state) || cJSON_IsNumber(act_state))) {
                bool state = cJSON_IsTrue(act_state) || (cJSON_IsNumber(act_state) && act_state->valueint != 0);
                ESP_LOGW(TAG, "Rule Triggered: %s %s %.1f. Activating Actuator %d -> %d",
                         field->valuestring, op->valuestring, val->valuedouble, act_id->valueint, state);
                hal_actuator_set((actuator_id_t)act_id->valueint, state);
            }
        }
    }

    cJSON_Delete(rules);
}
