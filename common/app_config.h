/**
 * @file app_config.h
 * @brief Compile-time configuration for ESP Thread Sensor Network.
 *
 * Override any of these in sdkconfig or via Kconfig if you want menu-driven
 * configuration.  The defaults work out-of-the-box on the ESP Thread BR board.
 */

#pragma once

/* ─── Firmware version ────────────────────────────────────────────────────── */
#define APP_FW_MAJOR    1
#define APP_FW_MINOR    0
#define APP_FW_PATCH    0

/* ─── Network ID  (shared by all nodes and the BR) ────────────────────────── */
#ifndef APP_NETWORK_ID
#define APP_NETWORK_ID  "HomeNet01"
#endif

/* ─── Border Router CoAP server address (Thread mesh-local EID) ───────────── */
/* This is discovered via SRP/DNS-SD at runtime; the define is a fallback.     */
#ifndef BR_COAP_ADDR
#define BR_COAP_ADDR    "fd00::1"   /* Thread mesh-local prefix + ::1          */
#endif
#define BR_COAP_PORT    5683

/* ─── MQTT broker ─────────────────────────────────────────────────────────── */
#ifndef MQTT_BROKER_URI
#define MQTT_BROKER_URI "mqtt://192.168.1.10:1883"   /* Change to your broker  */
#endif
#define MQTT_USERNAME   ""
#define MQTT_PASSWORD   ""
#define MQTT_QOS        1
#define MQTT_RETAIN     0
#define MQTT_KEEPALIVE_S  60

/* ─── Sensor node timing ──────────────────────────────────────────────────── */
#define SENSOR_REPORT_INTERVAL_S    30      /**< Normal report interval (sec)  */
#define SENSOR_FAST_INTERVAL_S       5      /**< Fast interval after trigger   */
#define SENSOR_SLEEP_DURATION_S     28      /**< Deep-sleep between reports    */

/* ─── DHT22 / SHT31 GPIO (sensor node) ────────────────────────────────────── */
#define SENSOR_DHT22_GPIO           GPIO_NUM_8  /* Change to your wiring       */
#define SENSOR_SHT31_I2C_SDA        GPIO_NUM_6
#define SENSOR_SHT31_I2C_SCL        GPIO_NUM_7
#define SENSOR_SHT31_I2C_PORT       I2C_NUM_0
#define SENSOR_SHT31_I2C_FREQ_HZ    100000

/* ─── LED GPIO ────────────────────────────────────────────────────────────── */
#define APP_LED_GPIO                GPIO_NUM_2

/* ─── Battery ADC (0 = disabled / USB-powered) ────────────────────────────── */
#define BATTERY_ADC_CHANNEL         ADC_CHANNEL_0   /* GPIO1 on ESP32-H2       */
#define BATTERY_ADC_ATTEN           ADC_ATTEN_DB_11
#define BATTERY_DIVIDER_RATIO       2.0f            /* Resistor divider ratio  */
#define BATTERY_MV_LOW_THRESHOLD    3300u           /* Below = NODE_FLAG_BATTERY_LOW */

/* ─── CoAP retries ────────────────────────────────────────────────────────── */
#define COAP_MAX_RETRIES            3
#define COAP_RETRY_DELAY_MS         2000

/* ─── Device registry (border router) ────────────────────────────────────────*/
#define DEVICE_REGISTRY_MAX_NODES   32

/* ─── HTTP REST API (border router) ──────────────────────────────────────────*/
#define REST_API_PORT               8080
#define REST_API_MAX_CONNECTIONS    5

/* ─── Logging tags ────────────────────────────────────────────────────────── */
#define TAG_MAIN    "APP_MAIN"
#define TAG_SENSOR  "APP_SENSOR"
#define TAG_COAP    "APP_COAP"
#define TAG_MQTT    "APP_MQTT"
#define TAG_REST    "APP_REST"
#define TAG_DEV_MGR "APP_DEV_MGR"
#define TAG_SLEEP   "APP_SLEEP"
