# Node Software Specification

## 1. Overview
The IoT Node is an ultra-low-power edge device designed for battery/solar operation. It collects environmental data (temperature, humidity, soil moisture) and communicates via Zigbee or Thread to the Gateway.

**Target Chip**: ESP32-C6 (Native 802.15.4 + Wi-Fi 6 + BLE).

## 2. Power Management Strategy

Power consumption is the critical metric for the node.
*   **Deep Sleep**: The node spends >99% of its time in deep sleep mode. The CPU and Radio are powered off. Only the RTC (Real Time Clock) and ULP (Ultra Low Power) coprocessor remain active.
*   **Wake-up Triggers**:
    *   *Timer Wakeup*: Periodic waking (e.g., every 15 minutes) to sample sensors and report data.
    *   *External Interrupt*: Wake up immediately on critical events (e.g., a physical button press or a tamper switch).
*   **Fast Boot**: The system skips unnecessary bootloader checks when waking from deep sleep to minimize active time.

## 3. Software Components

### 3.1 `sensor_hub`
*   **Responsibility**: Acquires data from connected sensors (I2C, ADC, GPIO).
*   **Features**:
    *   Powers up sensors only right before sampling.
    *   Applies calibration curves and averaging to ensure data accuracy.
    *   Formats data into Zigbee/Thread payloads.

### 3.2 `power_manager`
*   **Responsibility**: Monitors battery health and handles sleep transitions.
*   **Features**:
    *   Measures battery voltage via ADC.
    *   Calculates State of Charge (SoC).
    *   Manages the ESP32 Deep Sleep API and configures wake stubs.

### 3.3 `radio_stack` (Zigbee End Device / Thread Sleepy End Device)
*   **Responsibility**: Maintains mesh network membership while maximizing sleep.
*   **Features**:
    *   Implements standard ZCL (Zigbee Cluster Library) clusters: Basic, Power Configuration, Temperature Measurement, Relative Humidity Measurement.
    *   Handles asynchronous polling of the parent router for pending messages upon waking up.

## 4. Operational Flow
1.  **Wake Up**: Triggered by RTC Timer.
2.  **Sample**: Initialize `sensor_hub`, read values, turn off sensor power.
3.  **Connect/Transmit**: Wake up the radio stack, transmit payload to the Gateway, poll for incoming OTA or config changes.
4.  **Sleep**: Configure next wake interval based on battery level (dynamic intervals) and enter Deep Sleep.
