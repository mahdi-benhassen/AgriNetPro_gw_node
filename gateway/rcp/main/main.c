#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_zigbee_core.h"
#include "driver/uart.h"
#include "driver/gpio.h"

static const char *TAG = "ZB_RCP";

#define RCP_UART_NUM            UART_NUM_1
#define RCP_UART_BAUD           460800
#define RCP_UART_TX_PIN         (GPIO_NUM_17)
#define RCP_UART_RX_PIN         (GPIO_NUM_18)
#define RCP_UART_RTS_PIN        (GPIO_NUM_19)
#define RCP_UART_CTS_PIN        (GPIO_NUM_20)

static void configure_rcp_uart(void)
{
    uart_config_t uart_config = {
        .baud_rate = RCP_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(RCP_UART_NUM, 2048, 2048, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(RCP_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(RCP_UART_NUM,
                                 RCP_UART_TX_PIN, RCP_UART_RX_PIN,
                                 RCP_UART_RTS_PIN, RCP_UART_CTS_PIN));

    ESP_LOGI(TAG, "RCP UART configured: baud=%d", RCP_UART_BAUD);
}

void app_main(void)
{
    ESP_LOGI(TAG, "AgriNetPro Zigbee RCP (ESP32-H2) starting...");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    configure_rcp_uart();

    esp_zb_platform_config_t platform_cfg = {
        .radio_config = {
            .radio_mode = ZB_RADIO_MODE_NATIVE,
        },
        .host_config = {
            .host_connection_mode = ZB_HOST_CONNECTION_MODE_UART,
            .uart_config = {
                .uart_port = RCP_UART_NUM,
                .baud_rate = RCP_UART_BAUD,
            },
        },
    };

    ESP_ERROR_CHECK(esp_zb_platform_config(&platform_cfg));

    ESP_LOGI(TAG, "RCP initialized. Waiting for Host (ESP32-S3) commands via UART...");

    esp_zb_stack_main_loop();
}
