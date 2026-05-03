#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* UART pin configuration for RS485 */
#define HAL_MODBUS_UART_NUM            UART_NUM_1
#define HAL_MODBUS_TX_PIN              GPIO_NUM_4
#define HAL_MODBUS_RX_PIN              GPIO_NUM_5
#define HAL_MODBUS_RTS_PIN             GPIO_NUM_6
#define HAL_MODBUS_BAUD_RATE           9600
#define HAL_MODBUS_PARITY              UART_PARITY_DISABLE
#define HAL_MODBUS_DATA_BITS           UART_DATA_8_BITS
#define HAL_MODBUS_STOP_BITS           UART_STOP_BITS_1

/**
 * @brief Initializes UART for RS485 half-duplex Modbus RTU Master mode.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t hal_modbus_init(void);

/**
 * @brief Reads holding registers from a Modbus slave device.
 * @param slave_addr The address of the target slave device (1-247).
 * @param reg_addr The starting register address (0-based).
 * @param count The number of registers to read (1-125).
 * @param out Pointer to buffer to store the read register values.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t hal_modbus_read_holding_registers(uint8_t slave_addr, uint16_t reg_addr, uint16_t count, uint16_t *out);

/**
 * @brief Writes a single register to a Modbus slave device.
 * @param slave_addr The address of the target slave device (1-247).
 * @param reg_addr The register address (0-based).
 * @param value The value to write to the register.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t hal_modbus_write_single_register(uint8_t slave_addr, uint16_t reg_addr, uint16_t value);

#ifdef __cplusplus
}
#endif
