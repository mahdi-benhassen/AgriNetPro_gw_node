#include "hal_sensor.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "HAL_SENSOR";

#define HAL_SENSOR_SAMPLE_COUNT  5
#define HAL_SENSOR_TEMP_MIN      15.0f
#define HAL_SENSOR_TEMP_MAX      35.0f
#define HAL_SENSOR_HUM_MIN       40.0f
#define HAL_SENSOR_HUM_MAX       80.0f
#define HAL_SENSOR_CAL_OFFSET    0.0f

esp_err_t hal_sensor_init(void)
{
    ESP_LOGI(TAG, "Initializing I2C sensors (Mock)");
    vTaskDelay(pdMS_TO_TICKS(10));
    return ESP_OK;
}

static float read_sensor_simulated(float min_val, float max_val)
{
    uint32_t raw = esp_random() % 2000;
    return min_val + (float)raw / 100.0f * (max_val - min_val) / 20.0f;
}

static float read_sensor_averaged(float min_val, float max_val)
{
    float sum = 0.0f;

    for (int i = 0; i < HAL_SENSOR_SAMPLE_COUNT; i++) {
        sum += read_sensor_simulated(min_val, max_val);
    }

    return sum / (float)HAL_SENSOR_SAMPLE_COUNT;
}

esp_err_t hal_sensor_read_temperature(float *temp_c)
{
    if (temp_c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *temp_c = read_sensor_averaged(HAL_SENSOR_TEMP_MIN, HAL_SENSOR_TEMP_MAX);
    *temp_c += HAL_SENSOR_CAL_OFFSET;

    ESP_LOGD(TAG, "Read Temp: %.2f C", *temp_c);

    return ESP_OK;
}

esp_err_t hal_sensor_read_humidity(float *hum_rh)
{
    if (hum_rh == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *hum_rh = read_sensor_averaged(HAL_SENSOR_HUM_MIN, HAL_SENSOR_HUM_MAX);

    ESP_LOGD(TAG, "Read Humidity: %.2f %%", *hum_rh);

    return ESP_OK;
}

esp_err_t hal_sensor_deinit(void)
{
    ESP_LOGI(TAG, "Putting sensors to sleep (Mock)");
    return ESP_OK;
}
