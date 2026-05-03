#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Starts the provisioning process using BLE as primary method.
 * Checks if credentials already exist in NVS before starting.
 * If BLE provisioning is not available, falls back to SoftAP.
 * @return esp_err_t ESP_OK on success, ESP_FAIL if provisioning failed.
 */
esp_err_t provisioning_start(void);

/**
 * @brief Checks if the provisioning process has completed.
 * @return true if provisioning is done, false otherwise.
 */
bool provisioning_is_done(void);

#ifdef __cplusplus
}
#endif
