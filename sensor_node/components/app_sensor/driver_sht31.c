#include "app_sensor_driver.h"
#include "app_config.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SHT31_DRV";

#define SHT31_ADDR          0x44
#define SHT31_CMD_MEASURE   0x2C06
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

static esp_err_t sht31_init(void)
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

static esp_err_t sht31_read(app_sensor_reading_t *out)
{
    esp_err_t ret = sht31_write_cmd(SHT31_CMD_MEASURE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SHT31 trigger failed: %s", esp_err_to_name(ret));
        out->valid = false;
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(SHT31_MEAS_DELAY_MS));

    uint8_t buf[6];
    ret = i2c_master_read_from_device(SENSOR_SHT31_I2C_PORT,
                                      SHT31_ADDR, buf, 6,
                                      pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SHT31 read failed: %s", esp_err_to_name(ret));
        out->valid = false;
        return ret;
    }

    if (sht31_crc(buf, 2) != buf[2] || sht31_crc(buf + 3, 2) != buf[5]) {
        ESP_LOGE(TAG, "SHT31 CRC error");
        out->valid = false;
        return ESP_ERR_INVALID_CRC;
    }

    uint16_t t_raw = ((uint16_t)buf[0] << 8) | buf[1];
    uint16_t h_raw = ((uint16_t)buf[3] << 8) | buf[4];

    out->temperature_c = -45.0f + 175.0f * (float)t_raw / 65535.0f;
    out->humidity_pct  = 100.0f * (float)h_raw / 65535.0f;
    out->valid         = true;

    ESP_LOGI(TAG, "SHT31 → T=%.2f°C  RH=%.2f%%",
             out->temperature_c, out->humidity_pct);
    return ESP_OK;
}

static esp_err_t sht31_deinit(void)
{
    i2c_driver_delete(SENSOR_SHT31_I2C_PORT);
    return ESP_OK;
}

const app_sensor_driver_t sensor_driver_sht31 = {
    .init   = sht31_init,
    .read   = sht31_read,
    .deinit = sht31_deinit,
};
