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
#include "app_config.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>

static const char *TAG = TAG_SENSOR;

/* ─── Internal state ──────────────────────────────────────────────────────── */
static app_sensor_reading_t s_last = { .temperature_c = 0.0f,
                                       .humidity_pct  = 0.0f,
                                       .valid         = false };
static adc_oneshot_unit_handle_t s_adc_handle = NULL;

/* ══════════════════════════════════════════════════════════════════════════════
 * DHT22 bit-bang driver
 * Protocol:
 *   Host pulls LOW ≥ 1 ms  → DHT22 pulls LOW 80 µs, HIGH 80 µs → 40 bits
 *   Each bit: LOW 50 µs + HIGH (26-28 µs = '0', 70 µs = '1')
 * ══════════════════════════════════════════════════════════════════════════════ */
#ifdef CONFIG_APP_SENSOR_TYPE_DHT22

#define DHT_GPIO            SENSOR_DHT22_GPIO
#define DHT_TIMEOUT_US      1000

static int dht_wait_for_level(int level, uint32_t timeout_us)
{
    uint32_t elapsed = 0;
    while (gpio_get_level(DHT_GPIO) != level) {
        if (elapsed >= timeout_us) return -1;
        esp_rom_delay_us(1);
        elapsed++;
    }
    return (int)elapsed;
}

static esp_err_t dht22_read_raw(uint8_t data[5])
{
    /* Start signal: host drives LOW for 1 ms */
    gpio_set_direction(DHT_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(DHT_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(2));   /* ≥ 1 ms */
    gpio_set_level(DHT_GPIO, 1);
    esp_rom_delay_us(30);
    gpio_set_direction(DHT_GPIO, GPIO_MODE_INPUT);

    /* Wait for DHT response */
    if (dht_wait_for_level(0, 100) < 0) return ESP_ERR_TIMEOUT;
    if (dht_wait_for_level(1, 100) < 0) return ESP_ERR_TIMEOUT;
    if (dht_wait_for_level(0, 100) < 0) return ESP_ERR_TIMEOUT;

    /* Read 40 bits */
    memset(data, 0, 5);
    for (int i = 0; i < 40; i++) {
        if (dht_wait_for_level(1, 70) < 0) return ESP_ERR_TIMEOUT;
        int high_us = dht_wait_for_level(0, 90);
        if (high_us < 0) return ESP_ERR_TIMEOUT;
        if (high_us > 40) {
            data[i / 8] |= (1 << (7 - (i % 8)));   /* '1' bit */
        }
    }

    /* Checksum */
    uint8_t chk = data[0] + data[1] + data[2] + data[3];
    if (chk != data[4]) {
        ESP_LOGE(TAG, "DHT22 checksum fail: got 0x%02X expected 0x%02X",
                 chk, data[4]);
        return ESP_ERR_INVALID_CRC;
    }
    return ESP_OK;
}

esp_err_t app_sensor_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << DHT_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io), TAG, "GPIO config failed");
    ESP_LOGI(TAG, "DHT22 sensor ready on GPIO %d", DHT_GPIO);
    return ESP_OK;
}

esp_err_t app_sensor_read(app_sensor_reading_t *out)
{
    uint8_t data[5] = {0};
    /* DHT22 needs ≥ 2 s between reads */
    vTaskDelay(pdMS_TO_TICKS(100));

    esp_err_t ret = dht22_read_raw(data);
    if (ret != ESP_OK) {
        out->valid = false;
        ESP_LOGW(TAG, "DHT22 read failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Decode humidity */
    uint16_t hum_raw  = ((uint16_t)data[0] << 8) | data[1];
    /* Decode temperature (bit 15 = sign) */
    uint16_t temp_raw = ((uint16_t)(data[2] & 0x7F) << 8) | data[3];
    int sign = (data[2] & 0x80) ? -1 : 1;

    out->humidity_pct  = hum_raw  / 10.0f;
    out->temperature_c = sign * temp_raw / 10.0f;
    out->valid         = true;

    s_last = *out;
    ESP_LOGI(TAG, "DHT22 → T=%.1f°C  RH=%.1f%%",
             out->temperature_c, out->humidity_pct);
    return ESP_OK;
}

#endif /* CONFIG_APP_SENSOR_TYPE_DHT22 */


/* ══════════════════════════════════════════════════════════════════════════════
 * SHT31 I2C driver
 * Datasheet: https://www.sensirion.com/media/documents/213E6A3B/63A5A569/Datasheet_SHT3x_DIS.pdf
 * ══════════════════════════════════════════════════════════════════════════════ */
#ifdef CONFIG_APP_SENSOR_TYPE_SHT31

#define SHT31_ADDR          0x44        /* ADDR pin = GND → 0x44, VDD → 0x45  */
#define SHT31_CMD_MEASURE   0x2C06      /* Single-shot, high repeatability     */
#define SHT31_MEAS_DELAY_MS 20

static esp_err_t sht31_write_cmd(uint16_t cmd)
{
    uint8_t buf[2] = { (uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF) };
    return i2c_master_write_to_device(SENSOR_SHT31_I2C_PORT,
                                      SHT31_ADDR, buf, 2,
                                      pdMS_TO_TICKS(100));
}

static uint8_t sht31_crc(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
        }
    }
    return crc;
}

esp_err_t app_sensor_init(void)
{
    i2c_config_t cfg = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = SENSOR_SHT31_I2C_SDA,
        .scl_io_num       = SENSOR_SHT31_I2C_SCL,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = SENSOR_SHT31_I2C_FREQ_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_param_config(SENSOR_SHT31_I2C_PORT, &cfg),
                        TAG, "I2C param config");
    ESP_RETURN_ON_ERROR(i2c_driver_install(SENSOR_SHT31_I2C_PORT,
                                           I2C_MODE_MASTER, 0, 0, 0),
                        TAG, "I2C driver install");

    /* Soft-reset the sensor */
    ESP_RETURN_ON_ERROR(sht31_write_cmd(0x30A2), TAG, "SHT31 soft reset");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_LOGI(TAG, "SHT31 sensor ready (I2C addr 0x%02X)", SHT31_ADDR);
    return ESP_OK;
}

esp_err_t app_sensor_read(app_sensor_reading_t *out)
{
    /* Trigger single-shot high-repeatability measurement */
    esp_err_t ret = sht31_write_cmd(SHT31_CMD_MEASURE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SHT31 trigger failed: %s", esp_err_to_name(ret));
        out->valid = false;
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(SHT31_MEAS_DELAY_MS));

    /* Read 6 bytes: T_MSB T_LSB T_CRC H_MSB H_LSB H_CRC */
    uint8_t buf[6];
    ret = i2c_master_read_from_device(SENSOR_SHT31_I2C_PORT,
                                      SHT31_ADDR, buf, 6,
                                      pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SHT31 read failed: %s", esp_err_to_name(ret));
        out->valid = false;
        return ret;
    }

    /* Verify CRCs */
    if (sht31_crc(buf, 2) != buf[2] || sht31_crc(buf + 3, 2) != buf[5]) {
        ESP_LOGE(TAG, "SHT31 CRC error");
        out->valid = false;
        return ESP_ERR_INVALID_CRC;
    }

    uint16_t t_raw = ((uint16_t)buf[0] << 8) | buf[1];
    uint16_t h_raw = ((uint16_t)buf[3] << 8) | buf[4];

    /* Datasheet formulae */
    out->temperature_c = -45.0f + 175.0f * (float)t_raw / 65535.0f;
    out->humidity_pct  = 100.0f * (float)h_raw / 65535.0f;
    out->valid         = true;

    s_last = *out;
    ESP_LOGI(TAG, "SHT31 → T=%.2f°C  RH=%.2f%%",
             out->temperature_c, out->humidity_pct);
    return ESP_OK;
}

#endif /* CONFIG_APP_SENSOR_TYPE_SHT31 */


/* ─── Common implementations ──────────────────────────────────────────────── */

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
