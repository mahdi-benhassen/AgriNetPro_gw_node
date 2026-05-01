#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the sensor hardware (e.g., I2C bus).
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t hal_sensor_init(void);

/**
 * @brief Reads temperature from the sensor.
 * @param[out] temp_c Pointer to store the read temperature in Celsius.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t hal_sensor_read_temperature(float *temp_c);

/**
 * @brief Reads humidity from the sensor.
 * @param[out] hum_rh Pointer to store the read humidity in percentage.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t hal_sensor_read_humidity(float *hum_rh);

/**
 * @brief De-initializes the sensor hardware and puts it to sleep to save power.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t hal_sensor_deinit(void);

#ifdef __cplusplus
}
#endif
