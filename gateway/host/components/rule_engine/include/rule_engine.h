#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float temperature;
    float humidity;
} node_telemetry_t;

/**
 * @brief Evaluates telemetry data against configured local rules.
 * @param node_id The ID of the node that sent the data.
 * @param telemetry The telemetry payload.
 */
void rule_engine_evaluate(uint32_t node_id, const node_telemetry_t *telemetry);

#ifdef __cplusplus
}
#endif
