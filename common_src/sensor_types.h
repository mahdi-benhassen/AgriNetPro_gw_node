#ifndef SENSOR_TYPES_H
#define SENSOR_TYPES_H

#include <stdint.h>
#include <time.h>

// ============= Sensor Types =============
#define SENSOR_TYPE_TEMPERATURE 0x01
#define SENSOR_TYPE_HUMIDITY 0x02
#define SENSOR_TYPE_PRESSURE 0x03
#define SENSOR_TYPE_LIGHT 0x04

// ============= Network Status =============
#define NETWORK_STATUS_DISCONNECTED 0x00
#define NETWORK_STATUS_CONNECTING 0x01
#define NETWORK_STATUS_CONNECTED 0x02

// ============= Thread Device Types =============
#define THREAD_DEVICE_TYPE_NODE 0x01
#define THREAD_DEVICE_TYPE_GATEWAY 0x02

// ============= Data Structures =============

typedef struct {
    uint8_t type;           // Sensor type
    float value;            // Sensor reading value
    float min_value;        // Min expected value
    float max_value;        // Max expected value
    uint32_t timestamp;     // Reading timestamp (seconds since boot)
    uint8_t accuracy;       // Accuracy in percent (0-100)
} SensorReading_t;

typedef struct {
    uint64_t device_id;     // Unique device identifier
    uint8_t device_type;    // Node or Gateway
    uint8_t firmware_version;
    uint8_t num_sensors;
    uint8_t sensors[4];     // Types of sensors on this device
} DeviceInfo_t;

typedef struct {
    uint64_t node_id;
    uint32_t last_seen;     // Timestamp of last message
    uint8_t num_readings;
    SensorReading_t readings[100];  // Circular buffer
    uint8_t read_index;     // Current index in circular buffer
} NodeData_t;

typedef struct {
    uint8_t status;         // Network status
    uint8_t link_quality;   // 0-255 (0 = poor, 255 = excellent)
    uint32_t uptime_ms;     // Device uptime in milliseconds
    uint32_t message_count; // Total messages sent/received
    uint32_t error_count;   // Number of errors
} NetworkStats_t;

// ============= Sensor Driver Interface =============

typedef struct {
    const char *name;
    int (*init)(void);
    int (*read)(float *value);
    void (*deinit)(void);
} TempSensorDriver_t;

typedef struct {
    const char *name;
    int (*init)(void);
    int (*read)(float *value);
    void (*deinit)(void);
} HumiditySensorDriver_t;

// ============= Function Prototypes =============

TempSensorDriver_t *get_temperature_driver(void);
HumiditySensorDriver_t *get_humidity_driver(void);

#endif // SENSOR_TYPES_H
