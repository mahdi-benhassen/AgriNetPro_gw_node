/**
 * @file esp_ot_config.h
 * @brief OpenThread platform configuration defaults for the sensor node.
 *
 * In ESP-IDF v5.5+ the old "esp_openthread_defaults.h" system header was
 * removed.  The DEFAULT_* macros are now expected to be provided by the
 * application.
 *
 * The ESP32-H2 uses the native 802.15.4 radio — no UART/SPI RCP needed.
 */

#pragma once

#include "sdkconfig.h"
#include "esp_openthread_types.h"

/* ─── Radio configuration (native 802.15.4 on ESP32-H2) ──────────────── */
#define ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG()            \
    {                                                    \
        .radio_mode = RADIO_MODE_NATIVE,                 \
    }

/* ─── Host connection (none — standalone sensor node) ─────────────────── */
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
