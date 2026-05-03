#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes Wi-Fi and connects to the AP.
 * Credentials are loaded from NVS; falls back to defaults if not set.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t network_manager_init(void);

/**
 * @brief Saves WiFi credentials to NVS for future boots.
 * @param ssid WiFi network name (max 31 chars).
 * @param password WiFi password (max 63 chars).
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t network_manager_set_credentials(const char *ssid, const char *password);

/**
 * @brief Publishes telemetry data to the cloud MQTT broker over TLS.
 * Offline buffering to SPIFFS is automatic when disconnected.
 * @param topic MQTT topic string.
 * @param payload JSON formatted payload.
 * @return esp_err_t ESP_OK on success, ESP_FAIL if buffering fails.
 */
esp_err_t network_manager_publish_telemetry(const char *topic, const char *payload);

#ifdef __cplusplus
}
#endif
