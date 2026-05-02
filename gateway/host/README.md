# AgriNetPro Gateway (Host)

This project runs on the ESP32-S3 and acts as the Host for the AgriNetPro Dual-SoC Gateway.

## Architecture
- **Host**: ESP32-S3 (Wi-Fi, MQTT, Rule Engine, Zigbee Host Stack)
- **RCP**: ESP32-H2 (802.15.4 Radio)

## Configuration
The Zigbee stack is configured to communicate with the ESP32-H2 RCP via UART.
- **UART Port**: 1
- **TX Pin**: 17
- **RX Pin**: 18
- **Baudrate**: 115200

## How to Build
1.  Set target:
    ```bash
    idf.py set-target esp32s3
    ```
2.  Build and flash:
    ```bash
    idf.py build flash
    ```

## Features
- Wi-Fi STA mode for cloud connectivity.
- MQTT client for telemetry publishing.
- Zigbee Coordinator role (via RCP).
- Rule Engine for local automation.
