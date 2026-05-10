#pragma once
#include "app_sensor.h"

/**
 * @brief HAL interface that every sensor driver must implement.
 */
typedef struct {
    esp_err_t (*init)(void);
    esp_err_t (*read)(app_sensor_reading_t *out);
    esp_err_t (*deinit)(void);
} app_sensor_driver_t;

extern const app_sensor_driver_t sensor_driver_dht22;
extern const app_sensor_driver_t sensor_driver_sht31;
