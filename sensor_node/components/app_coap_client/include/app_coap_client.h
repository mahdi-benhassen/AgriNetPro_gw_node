/**
 * @file app_coap_client.h
 * @brief CoAP client for the sensor node.
 *
 * Handles:
 *  1. Node registration on first boot (POST /sensor/register)
 *  2. Periodic telemetry upload (POST /sensor/data)
 *  3. Polling for downlink commands (GET /cmd)
 *  4. OTA notification receipt
 *
 * The border router CoAP server address is resolved via DNS-SD (mDNS over
 * Thread) on first use; the result is cached and refreshed every hour.
 */

#pragma once

#include "esp_err.h"
#include "app_sensor.h"
#include "app_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise CoAP client.
 *
 * Creates internal CoAP context. Call after Thread network is attached.
 * @return ESP_OK on success.
 */
esp_err_t app_coap_client_init(void);

/**
 * @brief Register this node with the border router.
 *
 * Sends a POST /sensor/register with app_node_reg_payload_t.
 * Safe to call multiple times — the BR is idempotent on EUI-64.
 *
 * @param label  Human-readable name shown in the dashboard.
 * @return ESP_OK if the BR acknowledged.
 */
esp_err_t app_coap_client_register(const char *label);

/**
 * @brief Upload a sensor reading to the border router.
 *
 * Sends POST /sensor/data with a packed app_sensor_payload_t.
 * Retries up to COAP_MAX_RETRIES times on failure.
 *
 * @param reading  Fresh sensor reading from app_sensor_read().
 * @return ESP_OK on acknowledged delivery.
 */
esp_err_t app_coap_client_send(const app_sensor_reading_t *reading);

/**
 * @brief Poll the border router for a pending command.
 *
 * Sends GET /cmd. If a command is waiting the handler fires synchronously
 * within this call.
 *
 * @param[out] cmd  Filled when a command is present; cmd_type == CMD_NONE
 *                  if no command is pending.
 * @return ESP_OK (even when no command is pending).
 */
esp_err_t app_coap_client_poll_cmd(app_cmd_payload_t *cmd);

/**
 * @brief Deinitialise and free CoAP resources.
 */
void app_coap_client_deinit(void);

#ifdef __cplusplus
}
#endif
