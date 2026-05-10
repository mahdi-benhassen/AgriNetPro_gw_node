/**
 * @file app_sensor.c
 * @brief Sensor driver: DHT22 (1-wire bit-bang) + SHT31 (I2C).
 *
 * Build-time selection via Kconfig:
 *   CONFIG_APP_SENSOR_TYPE_DHT22
 *   CONFIG_APP_SENSOR_TYPE_SHT31
 *
 * Both sensors are wrapped under the same public API so that the CoAP/MQTT
 * layers never need to know which physical sensor is fitted.
 */

#include "app_sensor.h"
#include "app_sensor_driver.h"
#include "app_config.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

static const char *TAG = TAG_SENSOR;

static const app_sensor_driver_t *s_driver = 
#if defined(CONFIG_APP_SENSOR_TYPE_DHT22)
    &sensor_driver_dht22;
#elif defined(CONFIG_APP_SENSOR_TYPE_SHT31)
    &sensor_driver_sht31;
#else
    NULL;
#endif

static app_sensor_reading_t s_last = { .temperature_c = 0.0f,
                                       .humidity_pct  = 0.0f,
                                       .valid         = false };

static adc_oneshot_unit_handle_t s_adc_handle = NULL;

esp_err_t app_sensor_init(void)
{
    if (!s_driver || !s_driver->init) {
        ESP_LOGE(TAG, "No valid sensor driver selected");
        return ESP_ERR_NOT_SUPPORTED;
    }
    return s_driver->init();
}

esp_err_t app_sensor_read(app_sensor_reading_t *out)
{
    if (!s_driver || !s_driver->read) return ESP_ERR_NOT_SUPPORTED;
    
    esp_err_t err = s_driver->read(out);
    if (err == ESP_OK) {
        s_last = *out;
    } else {
        out->valid = false;
    }
    return err;
}

app_sensor_reading_t app_sensor_last_reading(void)
{
    return s_last;
}

uint16_t app_sensor_battery_mv(void)
{
#if CONFIG_APP_BATTERY_SENSE_ENABLE
    if (!s_adc_handle) {
        adc_oneshot_unit_init_cfg_t adc_cfg = { .unit_id = ADC_UNIT_1 };
        if (adc_oneshot_new_unit(&adc_cfg, &s_adc_handle) != ESP_OK) return 0;

        adc_oneshot_chan_cfg_t chan_cfg = {
            .atten    = BATTERY_ADC_ATTEN,
            .bitwidth = ADC_BITWIDTH_12,
        };
        adc_oneshot_config_channel(s_adc_handle,
                                   BATTERY_ADC_CHANNEL, &chan_cfg);
    }

    int raw = 0;
    adc_oneshot_read(s_adc_handle, BATTERY_ADC_CHANNEL, &raw);

    /* Convert raw → mV (3.3V reference, 12-bit, then undo resistor divider) */
    float mv = (float)raw * 3300.0f / 4095.0f * BATTERY_DIVIDER_RATIO;
    return (uint16_t)mv;
#else
    return 0u;  /* USB powered */
#endif
}

