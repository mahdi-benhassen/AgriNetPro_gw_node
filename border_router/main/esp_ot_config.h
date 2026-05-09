/**
 * @file esp_ot_config.h
 * @brief OpenThread platform configuration defaults for the Border Router.
 *
 * In ESP-IDF v5.5+ the old "esp_openthread_defaults.h" system header was
 * removed.  The DEFAULT_* macros are now expected to be provided by the
 * application (exactly as the esp-thread-br examples do via their own
 * "esp_ot_config.h").
 *
 * These defaults configure:
 *   • UART RCP link to ESP32-H2 co-processor (UART1, 460 800 baud)
 *   • No host CLI connection (the BR runs headless)
 *   • NVS-backed dataset storage
 *
 * Pin assignments come from Kconfig (CONFIG_PIN_TO_RCP_*).
 * If those are not set, sensible defaults for the ESP-Thread-Border-Router
 * dev-kit are used.
 */

#pragma once

#include "sdkconfig.h"
#include "driver/uart.h"
#include "esp_openthread_types.h"

/* ─── RCP UART pin defaults (ESP-Thread-Border-Router DevKit) ──────────── */
#ifndef CONFIG_PIN_TO_RCP_TX
#define CONFIG_PIN_TO_RCP_TX    17   /* ESP32-S3 GPIO → RCP TX */
#endif

#ifndef CONFIG_PIN_TO_RCP_RX
#define CONFIG_PIN_TO_RCP_RX    18   /* ESP32-S3 GPIO → RCP RX */
#endif

/* ─── Radio configuration (UART to ESP32-H2 RCP) ─────────────────────── */
#define ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG()            \
    {                                                    \
        .radio_mode = RADIO_MODE_UART_RCP,               \
        .radio_uart_config = {                            \
            .port = 1,                                    \
            .uart_config =                                \
                {                                         \
                    .baud_rate  = 460800,                  \
                    .data_bits  = UART_DATA_8_BITS,       \
                    .parity     = UART_PARITY_DISABLE,    \
                    .stop_bits  = UART_STOP_BITS_1,       \
                    .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE, \
                    .rx_flow_ctrl_thresh = 0,              \
                    .source_clk = UART_SCLK_DEFAULT,      \
                },                                        \
            .rx_pin = CONFIG_PIN_TO_RCP_TX,               \
            .tx_pin = CONFIG_PIN_TO_RCP_RX,               \
        },                                                \
    }

/* ─── Host connection (none — headless border router) ─────────────────── */
#define ESP_OPENTHREAD_DEFAULT_HOST_CONFIG()              \
    {                                                    \
        .host_connection_mode = HOST_CONNECTION_MODE_NONE, \
    }

/* ─── Port / storage configuration ────────────────────────────────────── */
#define ESP_OPENTHREAD_DEFAULT_PORT_CONFIG()                                    \
    {                                                                          \
        .storage_partition_name = "nvs", .netif_queue_size = 10,               \
        .task_queue_size = 10,                                                 \
    }
