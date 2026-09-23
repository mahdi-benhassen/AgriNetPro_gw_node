/**
 * @file app_sleep.h / app_sleep.c
 * @brief Power management for the Thread sensor node (ESP32-H2).
 *
 * Strategy:
 *  - "Light" mode: keep Thread alive, use modem-sleep + auto light-sleep
 *  - "Deep" mode : save Thread dataset to NVS, deep-sleep, rejoin on wake
 *
 * Sleepy End Device (SED) is configured in Thread stack so the parent
 * buffers frames during sleep.
 */

#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SLEEP_MODE_LIGHT = 0,   /**< Modem-sleep, Thread stays attached          */
    SLEEP_MODE_DEEP  = 1,   /**< Deep-sleep; Thread must rejoin on wake       */
} app_sleep_mode_t;

/**
 * @brief Initialise sleep subsystem.
 * Configures Thread as a Sleepy End Device (SED / SSED).
 */
esp_err_t app_sleep_init(app_sleep_mode_t mode);

/**
 * @brief Enter sleep for the specified duration.
 *
 * Light-sleep: suspends this task for duration_ms using esp_sleep_enable_timer_wakeup().
 * Deep-sleep:  saves Thread credentials to NVS and calls esp_deep_sleep().
 *              DOES NOT RETURN in deep-sleep mode.
 *
 * @param duration_ms  Sleep duration in milliseconds.
 */
void app_sleep_enter(uint32_t duration_ms);

/**
 * @brief Check if current boot is caused by wakeup from deep sleep.
 * @return true if wake-from-deep-sleep boot, false for cold boot.
 */
bool app_sleep_is_wakeup_from_deep(void);

/**
 * @brief Configure Thread link mode as a Sleepy End Device (SED).
 * Call this AFTER OpenThread stack is initialized and attached.
 */
esp_err_t app_sleep_configure_sed(void);

/**
 * @brief Restore active Thread operational dataset from NVS on deep-sleep wake.
 * Call this after esp_openthread_init() and before esp_openthread_auto_start().
 */
esp_err_t app_sleep_restore_dataset(void);

#ifdef __cplusplus
}
#endif
