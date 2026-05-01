#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZIGBEE_NODE_ENDPOINT            10
#define ZIGBEE_NODE_CHANNEL_MASK        ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK
#define ZIGBEE_NODE_JOIN_TIMEOUT_SEC    30

typedef void (*zigbee_node_sleep_ready_cb_t)(void);

esp_err_t zigbee_node_init(zigbee_node_sleep_ready_cb_t sleep_cb);
esp_err_t zigbee_node_report_temperature(float temp_celsius);
esp_err_t zigbee_node_report_humidity(float humidity_pct);
esp_err_t zigbee_node_report_battery(uint32_t voltage_mv);
bool zigbee_node_is_connected(void);
void zigbee_node_signal_tx_done(void);

#ifdef __cplusplus
}
#endif
