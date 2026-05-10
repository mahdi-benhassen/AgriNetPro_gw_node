#include "app_sensor_driver.h"
#include "app_config.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include <string.h>

static const char *TAG = "DHT22_DRV";

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

static esp_err_t dht22_init(void)
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

static esp_err_t dht22_read(app_sensor_reading_t *out)
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

    ESP_LOGI(TAG, "DHT22 → T=%.1f°C  RH=%.1f%%",
             out->temperature_c, out->humidity_pct);
    return ESP_OK;
}

static esp_err_t dht22_deinit(void)
{
    gpio_reset_pin(DHT_GPIO);
    return ESP_OK;
}

const app_sensor_driver_t sensor_driver_dht22 = {
    .init   = dht22_init,
    .read   = dht22_read,
    .deinit = dht22_deinit,
};
