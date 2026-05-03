# Gateway Software Specification

## 1. Overview
The Gateway acts as the central hub, bridging the local mesh network (Zigbee/Thread) to the cloud/local network via Wi-Fi or Ethernet. It also evaluates local rules to ensure autonomous operation even when offline.

**Architecture**: Dual-SoC design.
- **Host (ESP32-S3)**: Handles application logic, Wi-Fi, MQTT, and Rule Engine.
- **RCP (ESP32-H2)**: Radio Co-Processor handling the 802.15.4 stack (Zigbee/Thread).

## 2. Software Components

### 2.1 `network_manager`
*   **Responsibility**: Manages all network interfaces (Wi-Fi Station/AP, Ethernet).
*   **Features**:
    *   Smart provisioning (BLE or SoftAP) for initial setup.
    *   Automatic connection recovery and fallback strategies.

### 2.2 `zigbee_coordinator` (Host Side)
*   **Responsibility**: Manages the Zigbee network by communicating with the ESP32-H2 RCP.
*   **Features**:
    *   UART communication with the RCP SoC.
    *   Network formation and steering.
    *   Device management and attribute reporting handling.

### 2.3 `mqtt_client_service`
*   **Responsibility**: Handles bi-directional telemetry and command traffic with the backend.
*   **Features**:
    *   QoS 1 message delivery for critical events.
    *   Offline buffering: Stores telemetry in SPIFFS/LittleFS when disconnected and uploads upon reconnection.
    *   Topic Structure: `agri/gw/{gw_id}/node/{node_id}/telemetry`

### 2.3 `rule_engine`
*   **Responsibility**: Executes local automations without cloud dependency.
*   **Features**:
    *   Parses JSON-based rule definitions (e.g., `IF node_1.temp > 30 THEN relay_1.on()`).
    *   Subscribes to local state changes via the Event Loop.

### 2.4 `hal_rs485` / `hal_modbus`
*   **Responsibility**: Interfacing with industrial equipment.
*   **Features**: Modbus RTU Master implementation for reading external industrial sensors or controlling PLCs.

## 3. State Machine (Main App)

1.  **BOOT**: Initialize NVS, HAL, read configurations.
2.  **PROVISIONING**: If no Wi-Fi credentials exist, start BLE/AP provisioning.
3.  **CONNECTING**: Establish Wi-Fi and connect to the MQTT broker. Initialize Zigbee/Thread network.
4.  **OPERATIONAL**: Normal operation. Route packets, evaluate rules, handle commands.
5.  **OTA_UPDATE**: Suspend non-critical tasks, download firmware, verify signature, reboot.
6.  **FAULT**: Safe state, log error to NVS, attempt recovery reboot.
