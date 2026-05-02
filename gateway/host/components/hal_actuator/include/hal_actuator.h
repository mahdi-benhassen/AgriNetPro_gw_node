#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ACTUATOR_WATER_VALVE_1 = 0,
    ACTUATOR_FAN_1,
    ACTUATOR_HEATER_1,
    ACTUATOR_MAX
} actuator_id_t;

/**
 * @brief Initializes all GPIOs associated with actuators.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t hal_actuator_init(void);

/**
 * @brief Sets the state of a specific actuator.
 * @param id The ID of the actuator.
 * @param state true for ON, false for OFF.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t hal_actuator_set(actuator_id_t id, bool state);

#ifdef __cplusplus
}
#endif
