# ESP32-H2 Zigbee RCP Firmware

This project provides the Radio Co-Processor (RCP) firmware for the ESP32-H2 SoC in the AgriNetPro Dual-SoC Gateway.

## Hardware Connections

| ESP32-S3 (Host) | ESP32-H2 (RCP) |
|-----------------|----------------|
| GPIO 17 (TX)    | GPIO 0 (RX)    |
| GPIO 18 (RX)    | GPIO 1 (TX)    |
| GND             | GND            |

## How to Flash

1.  Set the target to ESP32-H2:
    ```bash
    idf.py set-target esp32h2
    ```
2.  Build and flash:
    ```bash
    idf.py build flash
    ```

## Functionality
The RCP firmware initializes the 802.15.4 radio and waits for commands from the ESP32-S3 Host via UART at 115200 bps.
