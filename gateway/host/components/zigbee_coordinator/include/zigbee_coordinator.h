#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZIGBEE_COORD_ENDPOINT           1
#define ZIGBEE_COORD_CHANNEL_MASK       ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK

/* UART pins for ESP32-S3 Host <-> ESP32-H2 RCP link */
#define ZIGBEE_COORD_UART_NUM           UART_NUM_1
#define ZIGBEE_COORD_UART_BAUD          460800
#define ZIGBEE_COORD_UART_TX_PIN        (GPIO_NUM_17)
#define ZIGBEE_COORD_UART_RX_PIN        (GPIO_NUM_18)
#define ZIGBEE_COORD_UART_RTS_PIN       (GPIO_NUM_19)
#define ZIGBEE_COORD_UART_CTS_PIN       (GPIO_NUM_20)

/** Telemetry data received from a Zigbee End Device */
typedef struct {
    uint16_t short_addr;        /**< Network short address of the node */
    uint8_t  src_endpoint;      /**< Source endpoint of the report */
    float    temperature;       /**< Temperature in Celsius (NAN if not reported) */
    float    humidity;          /**< Humidity in percent (NAN if not reported) */
    uint32_t battery_mv;       /**< Battery voltage in mV (0 if not reported) */
} zigbee_coord_node_data_t;

/**
 * @brief Callback invoked when new telemetry is received from a node.
 * @param data Pointer to the received node data.
 */
typedef void (*zigbee_coord_data_cb_t)(const zigbee_coord_node_data_t *data);

/**
 * @brief Initializes the Zigbee Coordinator, forms the network.
 * @param data_cb Callback for incoming node telemetry.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t zigbee_coordinator_init(zigbee_coord_data_cb_t data_cb);

#ifdef __cplusplus
}
#endif
