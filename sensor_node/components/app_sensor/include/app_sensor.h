/**
 * @file app_sensor.h
 * @brief Sensor abstraction layer — supports DHT22 (1-wire) and SHT31 (I2C).
 *
 * The driver is selected at runtime via sdkconfig:
 *   CONFIG_APP_SENSOR_TYPE_DHT22  → uses DHT22 via GPIO bit-bang
 *   CONFIG_APP_SENSOR_TYPE_SHT31  → uses SHT31 via I2C
 *
 * Returns readings in floating-point Celsius and % RH.
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Single combined sensor reading.
 */
typedef struct {
    float    temperature_c;    /**< Temperature in degrees Celsius             */
    float    humidity_pct;     /**< Relative Humidity in %                     */
    bool     valid;            /**< false if the read failed (use last good)   */
} app_sensor_reading_t;

/**
 * @brief Initialise the sensor hardware.
 * Must be called once before app_sensor_read().
 *
 * @return ESP_OK on success, ESP_ERR_* on hardware failure.
 */
esp_err_t app_sensor_init(void);

/**
 * @brief Read temperature and humidity.
 *
 * Blocks for up to 300 ms (DHT22) or 20 ms (SHT31) while waiting for
 * the measurement to complete.
 *
 * @param[out] out  Populated with the fresh reading; .valid = false on error.
 * @return ESP_OK on success.
 */
esp_err_t app_sensor_read(app_sensor_reading_t *out);

/**
 * @brief Return the last successful reading without doing a new measurement.
 */
app_sensor_reading_t app_sensor_last_reading(void);

/**
 * @brief Read the battery voltage in millivolts via ADC.
 *
 * @return Battery voltage mV, or 0 if battery sensing is not supported.
 */
uint16_t app_sensor_battery_mv(void);

#ifdef __cplusplus
}
#endif
