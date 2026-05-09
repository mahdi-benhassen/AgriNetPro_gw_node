#ifndef CONFIG_H
#define CONFIG_H

// ============= Device Configuration =============
#define DEVICE_ID_NODE_1        0x0001000000000001
#define DEVICE_ID_NODE_2        0x0001000000000002
#define DEVICE_ID_GATEWAY       0x0002000000000001

// ============= Sensor Configuration =============
// Temperature sensor (NTC 10K Thermistor on GPIO32/ADC0)
#define TEMP_SENSOR_ADC_CHANNEL 0
#define TEMP_SENSOR_GPIO        32
#define TEMP_SENSOR_POLL_INTERVAL_MS 10000   // 10 seconds
#define TEMP_SENSOR_MIN_VALUE   -40.0f       // °C
#define TEMP_SENSOR_MAX_VALUE   125.0f       // °C
#define TEMP_SENSOR_ACCURACY    1            // ±1°C

// NTC Thermistor Steinhart-Hart coefficients
#define NTC_A 0.001129148f
#define NTC_B 0.0002349f
#define NTC_C 0.00000085f
#define NTC_REFERENCE_RESISTANCE 10000.0f    // 10K ohm

// Humidity sensor (Capacitive on GPIO33/ADC1)
#define HUMIDITY_SENSOR_ADC_CHANNEL 1
#define HUMIDITY_SENSOR_GPIO    33
#define HUMIDITY_SENSOR_POLL_INTERVAL_MS 10000  // 10 seconds
#define HUMIDITY_SENSOR_MIN_VALUE 20.0f         // %RH
#define HUMIDITY_SENSOR_MAX_VALUE 95.0f         // %RH
#define HUMIDITY_SENSOR_ACCURACY 3              // ±3%

// ADC Configuration
#define ADC_SAMPLE_COUNT    10  // Number of samples for averaging
#define ADC_MAX_VALUE       4095 // 12-bit ADC

// ============= Thread Network Configuration =============
#define THREAD_PAN_ID           0xDEAD
#define THREAD_CHANNEL          15
#define THREAD_MASTER_KEY       {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, \
                                 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}

// ============= CoAP Configuration =============
#define COAP_PORT               5683
#define COAP_MAX_PAYLOAD        256

// ============= Message Configuration =============
#define MESSAGE_QUEUE_SIZE      20
#define MESSAGE_TX_TIMEOUT_MS   1000

// ============= Gateway Configuration =============
#define MAX_NODES               10
#define READINGS_PER_NODE       100
#define NODE_TIMEOUT_MS         60000   // 60 seconds without message = timeout

// ============= Logging Configuration =============
#define LOG_LEVEL_VERBOSE       1
#define LOG_LEVEL_INFO          1
#define LOG_LEVEL_WARNING       1
#define LOG_LEVEL_ERROR         1

#endif // CONFIG_H
