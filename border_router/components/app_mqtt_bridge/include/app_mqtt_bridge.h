/**
 * @file app_mqtt_bridge.h
 * @brief MQTT bridge public API for the ESP Thread Border Router.
 *
 * Publishes Thread sensor telemetry to a cloud MQTT broker
 * and subscribes to downlink command topics.
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the MQTT bridge.
 *
 * Connects to the MQTT broker defined in app_config.h and registers
 * a telemetry callback with the device registry so that every new
 * sensor reading is automatically published as JSON.
 *
 * @return ESP_OK on success, ESP_FAIL if client init failed.
 */
esp_err_t app_mqtt_bridge_start(void);

/**
 * @brief Publish a node's online/offline status to MQTT.
 *
 * @param eui64  Node EUI-64.
 * @param online true = online, false = offline.
 */
void app_mqtt_bridge_publish_status(uint64_t eui64, bool online);

#ifdef __cplusplus
}
#endif
