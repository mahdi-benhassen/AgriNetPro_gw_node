/**
 * @file app_protocol.h
 * @brief Shared application protocol definitions for ESP Thread Sensor Network.
 *
 * Used by BOTH the sensor_node firmware and the border_router firmware.
 * Defines the binary payload format, CoAP URIs, and MQTT topics.
 *
 * Architecture overview:
 *
 *  [Sensor Node (ESP32-H2)]  ──CoAP/Thread──►  [Border Router (ESP32-S3)]  ──MQTT──►  [Cloud/Dashboard]
 *       DHT22 / SHT31                             esp-thread-br SDK                     AWS IoT / Mosquitto
 *
 * Sensor data flows upward.  Commands (OTA, config) flow downward via MQTT→CoAP.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Version ─────────────────────────────────────────────────────────────── */
#define APP_PROTO_VERSION       1u      /**< Increment when payload layout changes */

/* ─── Sensor type identifiers ─────────────────────────────────────────────── */
typedef enum {
    SENSOR_TYPE_TEMP_HUMIDITY   = 0x01, /**< DHT22 / SHT3x combined reading     */
    SENSOR_TYPE_TEMPERATURE     = 0x02, /**< Temperature-only sensor             */
    SENSOR_TYPE_HUMIDITY        = 0x03, /**< Humidity-only sensor                */
    SENSOR_TYPE_PRESSURE        = 0x04, /**< Barometric pressure (BMP280)        */
    SENSOR_TYPE_LIGHT           = 0x05, /**< Ambient light (BH1750)              */
    SENSOR_TYPE_CO2             = 0x06, /**< CO2 ppm (SCD40)                     */
    SENSOR_TYPE_MOTION          = 0x07, /**< PIR motion detector                 */
    SENSOR_TYPE_DOOR            = 0x08, /**< Reed switch door/window sensor      */
} app_sensor_type_t;

/* ─── Node status flags (bitmask) ─────────────────────────────────────────── */
#define NODE_FLAG_BATTERY_LOW   (1u << 0)   /**< Battery < 20 %                */
#define NODE_FLAG_SENSOR_ERROR  (1u << 1)   /**< Sensor read failed last cycle  */
#define NODE_FLAG_OTA_PENDING   (1u << 2)   /**< Waiting for OTA image          */
#define NODE_FLAG_SLEEPY        (1u << 3)   /**< Node uses SED (sleepy) mode    */

/* ─── CoAP resource URIs  (node → border router) ──────────────────────────── */
#define COAP_URI_SENSOR_DATA    "/sensor/data"      /**< POST sensor payload     */
#define COAP_URI_SENSOR_REG     "/sensor/register"  /**< POST node registration  */
#define COAP_URI_CMD_GET        "/cmd"              /**< GET pending commands     */
#define COAP_URI_OTA_NOTIFY     "/ota/notify"       /**< POST OTA ready signal   */

/* ─── MQTT Topics  (border router ↔ cloud) ────────────────────────────────── */
/* Format: esp/thread/<network_id>/<node_eui64>/<subtopic>                     */
#define MQTT_TOPIC_PREFIX           "esp/thread"
#define MQTT_SUBTOPIC_TELEMETRY     "telemetry"    /**< Sensor readings          */
#define MQTT_SUBTOPIC_STATUS        "status"       /**< Node online/offline      */
#define MQTT_SUBTOPIC_CMD_DOWN      "cmd/down"     /**< Cloud → node command     */
#define MQTT_SUBTOPIC_CMD_ACK       "cmd/ack"      /**< Node acks command        */
#define MQTT_SUBTOPIC_OTA           "ota"          /**< OTA firmware URL         */

/* Maximum topic length (bytes) */
#define MQTT_TOPIC_MAX_LEN          128u
#define MQTT_PAYLOAD_MAX_LEN        512u

/* ─── Packed sensor payload (sent over CoAP, also embedded in MQTT JSON) ──── */
/**
 * @brief Binary sensor payload — 28 bytes, little-endian.
 *
 * Packed with __attribute__((packed)) so the layout is identical on
 * both ESP32-H2 (ARM Cortex-M) and ESP32-S3 (Xtensa LX7).
 *
 * Float encoding: IEEE 754 single precision.
 * Temperatures: degrees Celsius × 100  (e.g. 2350 = 23.50 °C)
 * Humidity:     % RH × 100            (e.g. 6580 = 65.80 % RH)
 */
typedef struct __attribute__((packed)) {
    uint8_t     version;        /**< APP_PROTO_VERSION                         */
    uint8_t     sensor_type;    /**< app_sensor_type_t                         */
    uint8_t     node_flags;     /**< NODE_FLAG_* bitmask                       */
    uint8_t     _reserved;      /**< align to 4 bytes                          */
    uint64_t    eui64;          /**< IEEE EUI-64 of this node                  */
    uint32_t    uptime_s;       /**< Node uptime in seconds                    */
    int16_t     temperature_c;  /**< °C × 100  (–32768..32767)                 */
    uint16_t    humidity_pct;   /**< % RH × 100 (0..10000)                     */
    uint16_t    battery_mv;     /**< Battery voltage mV (0 = USB powered)      */
    int16_t     rssi_dbm;       /**< Last Thread link RSSI in dBm              */
    uint16_t    seq_num;        /**< Monotonic sequence number per node        */
    uint8_t     _pad[2];        /**< Reserved, set to 0                        */
} app_sensor_payload_t;

/* Compile-time size check: must be 28 bytes */
_Static_assert(sizeof(app_sensor_payload_t) == 28,
               "app_sensor_payload_t size mismatch — check padding!");

/* ─── Node registration payload (first-contact CoAP POST) ─────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t     version;            /**< APP_PROTO_VERSION                     */
    uint8_t     sensor_type;        /**< app_sensor_type_t                     */
    uint8_t     fw_major;           /**< Firmware major version                */
    uint8_t     fw_minor;           /**< Firmware minor version                */
    uint64_t    eui64;              /**< Node EUI-64                           */
    uint16_t    report_interval_s;  /**< Normal reporting interval (seconds)   */
    uint16_t    sleep_interval_s;   /**< Sleep duration between reports        */
    char        label[16];          /**< Human-readable label, null-terminated */
} app_node_reg_payload_t;

/* ─── Downlink command payload (CoAP GET response / MQTT cmd/down) ─────────── */
typedef enum {
    CMD_NONE            = 0x00,
    CMD_SET_INTERVAL    = 0x01,     /**< Change reporting interval             */
    CMD_REBOOT          = 0x02,     /**< Reboot the node                       */
    CMD_OTA_START       = 0x03,     /**< Start OTA, url in cmd_payload         */
    CMD_LED_ON          = 0x04,     /**< Turn on LED (debug)                   */
    CMD_LED_OFF         = 0x05,     /**< Turn off LED (debug)                  */
} app_cmd_type_t;

typedef struct __attribute__((packed)) {
    uint8_t     cmd_type;           /**< app_cmd_type_t                        */
    uint8_t     cmd_id;             /**< Unique ID for ACK matching            */
    uint16_t    cmd_param;          /**< Generic 16-bit parameter              */
    char        cmd_payload[60];    /**< Variable command payload (e.g. URL)   */
} app_cmd_payload_t;

/* ─── Helper macros ────────────────────────────────────────────────────────── */
#define TEMP_RAW_TO_FLOAT(raw)      ((float)(raw) / 100.0f)
#define HUM_RAW_TO_FLOAT(raw)       ((float)(raw) / 100.0f)
#define TEMP_FLOAT_TO_RAW(val)      ((int16_t)((val) * 100.0f))
#define HUM_FLOAT_TO_RAW(val)       ((uint16_t)((val) * 100.0f))

#ifdef __cplusplus
}
#endif
