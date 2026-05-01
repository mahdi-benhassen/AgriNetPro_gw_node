#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes Wi-Fi and connects to the AP.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t network_manager_init(void);

/**
 * @brief Publishes telemetry data to the cloud MQTT broker.
 * @param topic MQTT topic string.
 * @param payload JSON formatted payload.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t network_manager_publish_telemetry(const char *topic, const char *payload);

#ifdef __cplusplus
}
#endif
