#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configures deep sleep parameters.
 * @param sleep_duration_sec How long to sleep in seconds.
 */
void power_manager_configure_sleep(uint32_t sleep_duration_sec);

/**
 * @brief Enters deep sleep immediately.
 * This function never returns.
 */
void power_manager_enter_deep_sleep(void);

/**
 * @brief Reads battery voltage via ADC.
 * @param[out] voltage_mv Pointer to store the read voltage in millivolts.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t power_manager_read_battery(uint32_t *voltage_mv);

#ifdef __cplusplus
}
#endif
