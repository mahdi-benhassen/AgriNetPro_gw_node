/**
 * @file app_device_registry.h / app_device_registry.c
 * @brief In-memory device registry for the ESP Thread Border Router.
 *
 * Tracks every registered sensor node, caches the latest telemetry,
 * and provides lookup/iteration for the REST API and MQTT bridge.
 *
 * Thread-safe: all public functions acquire an internal mutex.
 */

#pragma once

#include "app_protocol.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEVICE_LABEL_LEN    17      /* 16 chars + null */
#define DEVICE_ADDR_LEN     40      /* IPv6 address string length */
#define DEVICE_ONLINE_TTL_S 120     /* Mark offline if no report for 2 min */

/**
 * @brief Represents a single registered sensor node.
 */
typedef struct {
    uint64_t                eui64;                      /**< Unique node ID        */
    char                    label[DEVICE_LABEL_LEN];    /**< Human-readable name   */
    char                    ipv6_addr[DEVICE_ADDR_LEN]; /**< Thread ML-EID         */
    app_sensor_type_t       sensor_type;
    uint8_t                 fw_major;
    uint8_t                 fw_minor;
    uint16_t                report_interval_s;
    bool                    online;                     /**< Seen within TTL       */
    time_t                  last_seen;                  /**< UNIX timestamp        */
    app_sensor_payload_t    last_reading;               /**< Last telemetry        */
    uint32_t                total_reports;              /**< All-time counter      */
    uint8_t                 pending_cmd_type;           /**< Queued downlink cmd   */
    uint8_t                 pending_cmd_id;
    uint16_t                pending_cmd_param;
    char                    pending_cmd_payload[60];
} app_device_entry_t;

/** @brief Callback invoked when a device reports new telemetry. */
typedef void (*app_device_telemetry_cb_t)(const app_device_entry_t *dev,
                                          const app_sensor_payload_t *payload);

/**
 * @brief Initialise the registry. Call once from app_main.
 */
esp_err_t app_device_registry_init(void);

/**
 * @brief Register or update a node from its registration payload.
 *
 * Creates a new entry if the EUI-64 is unknown; updates fields if known.
 *
 * @param reg       Registration payload from CoAP POST /sensor/register.
 * @param ipv6_str  IPv6 source address string (from CoAP session).
 * @return ESP_OK, or ESP_ERR_NO_MEM if the registry is full.
 */
esp_err_t app_device_registry_register(const app_node_reg_payload_t *reg,
                                        const char *ipv6_str);

/**
 * @brief Update telemetry for a node.
 *
 * Auto-registers the node if it sent data without a prior registration.
 *
 * @param payload  Sensor payload from CoAP POST /sensor/data.
 * @param ipv6_str IPv6 source address string.
 * @return ESP_OK on success.
 */
esp_err_t app_device_registry_update(const app_sensor_payload_t *payload,
                                      const char *ipv6_str);

/**
 * @brief Look up a device by EUI-64.
 *
 * @param[out] out  Filled with a snapshot of the entry. Thread-safe copy.
 * @return ESP_OK if found, ESP_ERR_NOT_FOUND otherwise.
 */
esp_err_t app_device_registry_get(uint64_t eui64, app_device_entry_t *out);

/**
 * @brief Queue a command for the next time the node polls.
 */
esp_err_t app_device_registry_queue_cmd(uint64_t eui64,
                                         app_cmd_type_t cmd_type,
                                         uint16_t param,
                                         const char *payload);

/**
 * @brief Pop the queued command for a node (called by CoAP server on GET /cmd).
 * Clears the pending command after returning it.
 *
 * @param[out] cmd  Populated if a command was pending; cmd_type = CMD_NONE otherwise.
 * @return ESP_OK.
 */
esp_err_t app_device_registry_pop_cmd(uint64_t eui64, app_cmd_payload_t *cmd);

/**
 * @brief Iterate all registered devices.
 *
 * @param cb        Callback called for each entry (with registry mutex held).
 * @param user_data Passed through to the callback.
 */
void app_device_registry_foreach(void (*cb)(const app_device_entry_t *dev,
                                             void *user_data),
                                  void *user_data);

/**
 * @brief Return the number of registered devices.
 */
int app_device_registry_count(void);

/**
 * @brief Register a callback invoked when new telemetry arrives.
 * Used by the MQTT bridge to publish immediately on receipt.
 */
void app_device_registry_set_telemetry_cb(app_device_telemetry_cb_t cb);

/**
 * @brief Sweep offline nodes (call periodically from a timer task).
 */
void app_device_registry_sweep_offline(void);

#ifdef __cplusplus
}
#endif
