/**
 * @file app_coap_server.h
 * @brief CoAP server public API for the ESP Thread Border Router.
 *
 * Listens on UDP port 5683 on the Thread mesh-local interface.
 * Handles:
 *   POST /sensor/register  → app_device_registry_register()
 *   POST /sensor/data      → app_device_registry_update()
 *   GET  /cmd              → app_device_registry_pop_cmd()
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the CoAP server task.
 *
 * Creates a FreeRTOS task that runs the CoAP event loop, listening on
 * BR_COAP_PORT (5683) for incoming sensor registration, telemetry,
 * and command-poll requests.
 *
 * @return ESP_OK on success, ESP_FAIL if task creation failed.
 */
esp_err_t app_coap_server_start(void);

#ifdef __cplusplus
}
#endif
