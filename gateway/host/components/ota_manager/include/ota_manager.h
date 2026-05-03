#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OTA update states.
 */
typedef enum {
    OTA_IDLE,          /**< No OTA operation in progress */
    OTA_IN_PROGRESS,   /**< OTA download and update in progress */
    OTA_SUCCESS,       /**< OTA completed successfully */
    OTA_FAILED         /**< OTA failed */
} ota_state_t;

/**
 * @brief Initializes OTA subsystem.
 * Sets initial state and prepares for OTA operations.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t ota_manager_init(void);

/**
 * @brief Starts OTA download from HTTPS URL.
 * Spawns a low-priority FreeRTOS task to perform the download.
 * @param firmware_url HTTPS URL to download firmware from.
 * @return esp_err_t ESP_OK if task started, error code on failure.
 */
esp_err_t ota_manager_start(const char *firmware_url);

/**
 * @brief Marks current boot partition as valid after successful connection.
 * Calls esp_ota_mark_app_valid_cancel_rollback() to prevent rollback.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t ota_manager_mark_valid(void);

/**
 * @brief Returns current OTA state.
 * @return ota_state_t Current OTA state.
 */
ota_state_t ota_manager_get_state(void);

#ifdef __cplusplus
}
#endif
