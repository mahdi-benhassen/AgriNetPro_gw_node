#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Sleep interval presets based on battery SoC */
#define POWER_MGR_SLEEP_NORMAL_SEC   900
#define POWER_MGR_SLEEP_LOW_SEC      1800
#define POWER_MGR_SLEEP_CRITICAL_SEC 3600

/** External wakeup GPIO (tamper switch / button) */
#define POWER_MGR_WAKEUP_GPIO        GPIO_NUM_0

/**
 * @brief Configures deep sleep with timer and optional external wakeup.
 * @param sleep_duration_sec How long to sleep in seconds.
 * @param enable_ext_wake If true, configure external interrupt on POWER_MGR_WAKEUP_GPIO.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t power_manager_configure_sleep(uint32_t sleep_duration_sec, bool enable_ext_wake);

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

/**
 * @brief Calculates battery State of Charge from voltage.
 * @param voltage_mv Battery voltage in millivolts.
 * @return Estimated SoC percentage (0-100).
 */
uint8_t power_manager_calc_soc(uint32_t voltage_mv);

/**
 * @brief Returns recommended sleep duration based on battery SoC.
 * @param voltage_mv Current battery voltage in millivolts.
 * @return Sleep duration in seconds.
 */
uint32_t power_manager_get_sleep_duration(uint32_t voltage_mv);

#ifdef __cplusplus
}
#endif
