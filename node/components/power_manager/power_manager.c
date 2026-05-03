#include "power_manager.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_random.h"
#include "driver/gpio.h"

static const char *TAG = "POWER_MGR";

#define BATTERY_VOLTAGE_MIN_MV   3200
#define BATTERY_VOLTAGE_MAX_MV   4200

esp_err_t power_manager_configure_sleep(uint32_t sleep_duration_sec, bool enable_ext_wake)
{
    ESP_LOGI(TAG, "Configuring timer wakeup for %lu seconds", sleep_duration_sec);
    esp_err_t ret = esp_sleep_enable_timer_wakeup((uint64_t)sleep_duration_sec * 1000000ULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure timer wakeup");
        return ret;
    }

    if (enable_ext_wake) {
        ESP_LOGI(TAG, "Configuring external wakeup on GPIO %d", POWER_MGR_WAKEUP_GPIO);
        gpio_config_t wkup_conf = {
            .pin_bit_mask = (1ULL << POWER_MGR_WAKEUP_GPIO),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .intr_type = GPIO_INTR_LOW_LEVEL,
        };
        gpio_config(&wkup_conf);

        ret = esp_sleep_enable_ext1_wakeup(BIT64(POWER_MGR_WAKEUP_GPIO), ESP_EXT1_WAKEUP_ANY_LOW);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure external wakeup");
            return ret;
        }
    }

    return ESP_OK;
}

void power_manager_enter_deep_sleep(void)
{
    ESP_LOGI(TAG, "Entering deep sleep...");
    esp_deep_sleep_start();
}

esp_err_t power_manager_read_battery(uint32_t *voltage_mv)
{
    if (voltage_mv == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Simulate ADC reading for a 3.7V LiPo (3.2V to 4.2V) */
    *voltage_mv = BATTERY_VOLTAGE_MIN_MV + (esp_random() % 1000);
    ESP_LOGD(TAG, "Read Battery: %lu mV", *voltage_mv);

    return ESP_OK;
}

uint8_t power_manager_calc_soc(uint32_t voltage_mv)
{
    if (voltage_mv >= BATTERY_VOLTAGE_MAX_MV) {
        return 100;
    }
    if (voltage_mv <= BATTERY_VOLTAGE_MIN_MV) {
        return 0;
    }
    return (uint8_t)((voltage_mv - BATTERY_VOLTAGE_MIN_MV) * 100 / (BATTERY_VOLTAGE_MAX_MV - BATTERY_VOLTAGE_MIN_MV));
}

uint32_t power_manager_get_sleep_duration(uint32_t voltage_mv)
{
    uint8_t soc = power_manager_calc_soc(voltage_mv);

    if (soc < 15) {
        ESP_LOGW(TAG, "Battery critical (%u%%), extending sleep to %d s",
                 soc, POWER_MGR_SLEEP_CRITICAL_SEC);
        return POWER_MGR_SLEEP_CRITICAL_SEC;
    } else if (soc < 40) {
        ESP_LOGI(TAG, "Battery low (%u%%), extending sleep to %d s",
                 soc, POWER_MGR_SLEEP_LOW_SEC);
        return POWER_MGR_SLEEP_LOW_SEC;
    }

    ESP_LOGD(TAG, "Battery OK (%u%%), normal sleep %d s",
             soc, POWER_MGR_SLEEP_NORMAL_SEC);
    return POWER_MGR_SLEEP_NORMAL_SEC;
}
