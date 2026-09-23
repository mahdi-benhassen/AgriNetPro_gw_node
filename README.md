# AgriNetPro — ESP Thread Sensor Network (Production Stack)

A production-ready, industrial-grade IoT application stack built on top of the [ESP Thread Border Router SDK](https://docs.espressif.com/projects/esp-thread-br/en/latest/) and [ESP-IDF v5.5+](https://github.com/espressif/esp-idf).

Designed for harsh, mission-critical environments such as **smart agriculture**, **smart poultry coops**, and **precision greenhouses**.

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                                APPLICATION LAYER & GATEWAY                             │
│  ┌────────────────────────┐  ┌───────────────────┐  ┌───────────────────────────────┐  │
│  │ React Live Dashboard   │  │   HTTP REST API   │  │  MQTT / MQTTS Bridge (TLS)    │  │
│  │ (Telemetry & OTA UI)   │  │  (Port 8080 JSON) │  │  (x509 Bundle / AWS / HiveMQ) │  │
│  └───────────▲────────────┘  └─────────▲─────────┘  └───────────────▲───────────────┘  │
│              │                         │                            │                  │
│  ┌───────────┴─────────────────────────┴────────────────────────────┴───────────────┐  │
│  │           Thread-Safe Device Registry (NVS Flash Persistence & Offline Sweep)    │  │
│  └─────────────────────────────────────▲────────────────────────────────────────────┘  │
│                                        │                                               │
│  ┌─────────────────────────────────────┴────────────────────────────────────────────┐  │
│  │           Dual-Port CoAP / CoAPS Server (Port 5683 UDP & Port 5684 DTLS PSK)     │  │
│  └─────────────────────────────────────▲────────────────────────────────────────────┘  │
├────────────────────────────────────────┼───────────────────────────────────────────────┤
│                     ESP Thread Border Router (ESP32-S3 Host)                           │
│  ┌─────────────────────────────────────▼────────────────────────────────────────────┐  │
│  │   ESP-IDF v5.5.4 + OpenThread 1.4 FTD + Wi-Fi / Ethernet Backbone Routing       │  │
│  └─────────────────────────────────────▲────────────────────────────────────────────┘  │
│                                        │ SPI Radio Interface                           │
│  ┌─────────────────────────────────────▼────────────────────────────────────────────┐  │
│  │   Radio Co-Processor (RCP) on ESP32-H2 (IEEE 802.15.4 @ 2.4 GHz)                 │  │
│  └──────────────────────────────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────┼───────────────────────────────────────────────┘
                                         │ Thread Mesh (802.15.4 / 6LoWPAN)
                                         │ Encrypted Mesh-Local UDP / DTLS
┌────────────────────────────────────────▼───────────────────────────────────────────────┐
│               Ultra-Low-Power Sensor Leaf Nodes (ESP32-H2 DevKit)                      │
│                                                                                        │
│  • Modular HAL: DHT22, SHT31-D, Soil Moisture, CO2, Light                              │
│  • Sleepy End Device (SED) with Automatic Modem-Sleep / Deep-Sleep Dataset Retention  │
│  • CoAP / CoAPS Client with Autonomous Retransmissions                                │
│  • Bidirectional Command & ACK Pipeline                                                │
│  • Remote Background OTA Engine (esp_https_ota) with Sleep Guard & Dual 1.94MB A/B     │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## Hardware Specification

| Role | Board / Module | Target MCU | Radio / Interfaces |
|---|---|---|---|
| **Border Router Host** | ESP Thread Border Router Board | **ESP32-S3** (Xtensa dual-core) | Wi-Fi 802.11 b/g/n + Ethernet (Backbone) |
| **Radio Co-Processor** | Built-in on Border Router Board | **ESP32-H2** (RISC-V single-core) | IEEE 802.15.4 (Thread 1.4 Radio via SPI) |
| **Sensor Leaf Node** | ESP32-H2 DevKitC-1 | **ESP32-H2** | IEEE 802.15.4 (Native Thread End Device) |
| **Sensors** | Digital Bus / Analog | — | DHT22 (1-wire), SHT31-D (I2C), ADC Battery |

### Sensor Node Default Pinout (ESP32-H2)

| Peripheral | Sensor / Feature | GPIO Pin | Notes |
|---|---|---|---|
| 1-Wire Bit-bang | DHT22 Data | `GPIO 8` | Requires 10 kΩ pull-up to 3.3V |
| I2C SDA | SHT31-D Data | `GPIO 1` | 100 kHz standard mode |
| I2C SCL | SHT31-D Clock | `GPIO 2` | 100 kHz standard mode |
| ADC1 Channel 4 | Battery Voltage Sensing | `GPIO 9` | 2:1 resistive divider (0–6.6V range) |
| Status Indicator | Onboard LED | `GPIO 8` | Configurable in `app_config.h` |

---

## Repository Layout

```
AgriNetPro_gw_node/
├── common/                                 # Shared protocol definitions & cross-target headers
│   ├── app_protocol.h                      # Binary payload structs, CoAP URIs, MQTT topics, ACKs
│   └── app_config.h                        # Network ID, ports, PSK credentials, timing defaults
│
├── sensor_node/                            # Leaf Node Firmware (ESP32-H2)
│   ├── CMakeLists.txt
│   ├── Kconfig.projbuild                   # Sensor selection, reporting intervals, sleep modes
│   ├── sdkconfig.defaults
│   ├── partitions.csv                      # Optimized 4MB Dual A/B OTA partition scheme (1.94 MB each)
│   ├── main/
│   │   ├── CMakeLists.txt
│   │   └── app_node_main.c                 # Cold/warm boot, Thread attach, OTA worker task, sleep loop
│   └── components/
│       ├── app_sensor/                     # Modular Sensor HAL (DHT22, SHT31-D, battery ADC)
│       ├── app_coap_client/                # libcoap client: POST data, GET cmd, POST ack (UDP/DTLS)
│       └── app_sleep/                      # Sleepy End Device (SED) config + NVS dataset caching
│
├── border_router/                          # Gateway Host Firmware (ESP32-S3)
│   ├── CMakeLists.txt
│   ├── Kconfig.projbuild
│   ├── sdkconfig.defaults
│   ├── partitions.csv
│   ├── main/
│   │   ├── CMakeLists.txt
│   │   └── app_br_main.c                   # Wi-Fi attach -> Thread BR init -> Registry -> CoAP -> MQTT -> REST
│   └── components/
│       ├── app_device_registry/            # Node registry, command queue, dirty-flag NVS flash persistence
│       ├── app_coap_server/                # Dual-stack server: UDP:5683 and DTLS:5684 (PSK)
│       ├── app_mqtt_bridge/                # MQTT client with x509 bundle TLS (telemetry, status, ack, down)
│       └── app_rest_api/                   # HTTP web server on port 8080 (REST JSON endpoints)
│
└── dashboard/
    └── ThreadMonitorDashboard.jsx          # React dashboard with live telemetry sparklines, ACKs & OTA triggers
```

---

## Architectural Deep Dive & Key Innovations

### 1. Registry Flash Persistence (NVS)
In dynamic agricultural environments subject to transient power drops, the Border Router persists all registered nodes, reporting intervals, and historical statistics to non-volatile storage (`app_reg` NVS namespace):
* **Fast In-Memory Operation:** Telemetry ingestion occurs in RAM under mutex protection for maximum throughput.
* **Flash Endurance Optimization:** Instead of writing to flash on every sensor reading, an intelligent dirty-flag (`s_dirty`) sweeps changed records during the periodic offline sweep (every 30 s), preserving flash life while guaranteeing state retention across power cycles.
* **Warm Reboot State Recovery:** Upon boot, `app_device_registry_load_from_nvs()` immediately restores known nodes, their labels, and network identifiers.

### 2. Bidirectional Command & End-to-End ACK Pipeline
Leaf nodes run in **Sleepy End Device (SED)** mode where radios are powered off between measurements. Downlink communication uses a store-and-forward polling pattern coupled with verified execution ACKs:
1. **Queueing:** Cloud or Dashboard submits command via MQTT (`esp/thread/<netid>/<eui64>/cmd/down`) or REST (`POST /api/v1/nodes/<eui64>/cmd`).
2. **Buffering:** Border Router queues the command in the node's registry slot.
3. **Polling:** Node awakens, sends telemetry, and queries `GET /cmd?eui=<eui64>`.
4. **Execution:** Node processes the command (e.g. interval change, LED toggle, reboot, or OTA launch).
5. **ACK Uplink:** Node formats a 44-byte binary `app_cmd_ack_payload_t` and sends `POST /cmd/ack`.
6. **Confirmation:** Border Router logs the result and publishes `esp/thread/<netid>/<eui64>/cmd/ack` back to MQTT subscribers and exposes `last_ack` in REST queries.

### 3. Remote Over-The-Air (OTA) Firmware Engine
Field nodes can be upgraded remotely without physical intervention:
* **Asynchronous Download:** Command `CMD_OTA_START` with a target firmware URL spawns `ota_worker_task` using `esp_https_ota`.
* **Sleep Suppression Guard:** During OTA execution, the node sets `s_ota_in_progress = true`, suppresses SED sleep (polling in active FreeRTOS delay), and marks `NODE_FLAG_OTA_PENDING` on outbound telemetry to notify the gateway.
* **A/B Dual Partitioning:** Flash layout allocates dual 1,984 KB partitions (`ota_0` and `ota_1`), safely accommodating the 1.16 MB monolithic binary (Thread + CoAP + DTLS + HTTPS).
* **Rollback & Verification:** If the new image fails boot validation, `esp_https_ota` automatically rolls back to the prior operational slot.

### 4. Transport Layer Security (CoAPS & MQTT TLS)
* **CoAPS DTLS PSK (Port 5684):** In addition to standard UDP port 5683, the border router binds a DTLS PSK endpoint. Sensor nodes configured with `CONFIG_APP_COAP_SEC_DTLS_PSK=y` establish encrypted, tamper-proof UDP sessions.
* **MQTTS TLS with ESP x509 Bundle:** The MQTT bridge checks broker URIs (`mqtts://` or `ssl://`) and attaches the root certificate bundle via `esp_crt_bundle_attach`, enabling direct and secure connection to cloud brokers like AWS IoT Core, HiveMQ Cloud, or Mosquitto TLS without hardcoding static PEM certificates.

---

## Sensor Node Partition Table (4MB Flash)

The sensor node partition table (`sensor_node/partitions.csv`) eliminates the legacy factory partition to maximize space for dual A/B OTA slots:

```csv
# Name,   Type, SubType, Offset,   Size,     Flags
nvs,      data, nvs,     0x9000,   0x6000,
otadata,  data, ota,     0xF000,   0x2000,
ota_0,    app,  ota_0,   0x10000,  0x1F0000,
ota_1,    app,  ota_1,   0x200000, 0x1F0000,
```

* **Usable App Space:** 1,984 KB (1.94 MB) per slot — plenty of headroom for future expansion.

---

## Getting Started: Build & Deployment

### Prerequisites
Install [ESP-IDF v5.5+](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/get-started/index.html):
```bash
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf && git checkout v5.5.4
./install.sh && . ./export.sh
cd ..

# Clone ESP Thread Border Router SDK
git clone --recursive https://github.com/espressif/esp-thread-br.git
export ESP_THREAD_BR_PATH=$(pwd)/esp-thread-br

# Clone AgriNetPro
git clone https://github.com/mahdi-benhassen/AgriNetPro_gw_node.git
cd AgriNetPro_gw_node
```

---

### Step 1: Build the Radio Co-Processor (RCP)
The RCP runs on the ESP32-H2 located on the Border Router Board:
```bash
cd $IDF_PATH/examples/openthread/ot_rcp
idf.py set-target esp32h2
idf.py build
# The ESP32-S3 host firmware will automatically flash and manage this image.
```

---

### Step 2: Build & Flash the Border Router (ESP32-S3)
```bash
cd AgriNetPro_gw_node/border_router
idf.py set-target esp32s3
idf.py menuconfig
# Configure:
#  - Example Connection Configuration -> WiFi SSID & Password
#  - AgriNetPro Configuration -> MQTT Broker URI (e.g., mqtts://broker.hivemq.com:8883)
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

#### First-Time Network Commissioning
In the Border Router monitor console:
```text
ot wifi connect -s "MyFarmWiFi" -p "FarmSecret123"
ot dataset init new
ot dataset commit active
ot ifconfig up
ot thread start

# Retrieve hex dataset for sensor node provisioning:
ot dataset active -x
```

---

### Step 3: Build & Flash Sensor Nodes (ESP32-H2)
```bash
cd AgriNetPro_gw_node/sensor_node
idf.py set-target esp32h2
idf.py menuconfig
# Configure:
#  - Node Label (e.g., "Greenhouse-North")
#  - Sensor selection (DHT22 or SHT31)
#  - Optional: Enable CoAPS DTLS PSK
idf.py build
idf.py -p /dev/ttyUSB1 flash monitor
```

#### Attach Node to the Mesh Network
Paste the dataset extracted from Step 2:
```text
ot dataset set active <hex-dataset>
ot dataset commit active
ot ifconfig up
ot thread start
```
The node will automatically join the network, register via `/sensor/register`, and begin cyclic telemetry publishing.

---

## HTTP REST API Reference

Base URL: `http://<border-router-ip>:8080`

### 1. List All Nodes
* **`GET /api/v1/nodes`**
* **Response:**
```json
[
  {
    "eui64": "48F17EC300A1B2C1",
    "label": "Greenhouse-North",
    "ipv6": "fd00:db8:a0:0:1a2b:3c4d:5e6f:7a8b",
    "online": true,
    "last_seen": 1727137200,
    "fw_version": 100,
    "report_interval_s": 30,
    "total_reports": 412,
    "reading": {
      "temperature_c": 24.2,
      "humidity_pct": 68.5,
      "battery_mv": 3740,
      "rssi_dbm": -58,
      "battery_low": false
    },
    "last_ack": {
      "cmd_id": 12,
      "cmd_type": 1,
      "status_code": 0,
      "message": "Interval set to 30s",
      "ts": 1727137210
    }
  }
]
```

### 2. Node Inspection
* **`GET /api/v1/nodes/<eui64>`**

### 3. Queue Downlink Command
* **`POST /api/v1/nodes/<eui64>/cmd`**
* **Headers:** `Content-Type: application/json`
* **Body:**
```json
{
  "cmd": 1,
  "param": 60,
  "data": null
}
```

#### Available Commands:
| Code | Identifier | Parameter (`param`) | Payload (`data`) | Description |
|---|---|---|---|---|
| `1` | `CMD_SET_INTERVAL` | Interval in seconds (5–3600) | `null` | Updates transmission frequency |
| `2` | `CMD_REBOOT` | `0` | `null` | Performs software reset |
| `3` | `CMD_OTA_START` | `0` | `"https://.../firmware.bin"` | Triggers remote firmware upgrade |
| `4` | `CMD_LED_ON` | `0` | `null` | Turns onboard LED on |
| `5` | `CMD_LED_OFF` | `0` | `null` | Turns onboard LED off |

### 4. Gateway System Status
* **`GET /api/v1/status`**
* **Response:**
```json
{
  "network_id": "AgriNetPro01",
  "uptime_s": 142850.5,
  "node_count": 8,
  "fw_version": "v1.0"
}
```

---

## MQTT Specification

Topic Pattern: `esp/thread/<network_id>/<eui64>/<subtopic>`

| Direction | Subtopic | QoS | Retain | Description |
|---|---|---|---|---|
| ↑ Outbound | `telemetry` | 1 | 0 | Periodic sensor readings |
| ↑ Outbound | `status` | 1 | 1 | Node online (`{"status":"online"}`) / offline |
| ↑ Outbound | `cmd/ack` | 1 | 0 | Command execution confirmation |
| ↓ Inbound | `cmd/down` | 1 | 0 | Cloud command dispatch |
| ↓ Inbound | `ota` | 1 | 0 | Cloud OTA URL dispatch |

### Command ACK Schema (`cmd/ack`)
```json
{
  "network_id": "AgriNetPro01",
  "eui64": "48F17EC300A1B2C1",
  "cmd_id": 15,
  "cmd_type": 3,
  "status_code": 0,
  "status": "success",
  "message": "OTA task launched",
  "ts": 1727137250
}
```

---

## Live React Dashboard

A modern, responsive monitoring interface is available at `dashboard/ThreadMonitorDashboard.jsx`:
* **Zero Configuration:** Automatically scans and polls nodes every 5 seconds.
* **Telemetry Sparklines:** Historical trends for microclimate temperature and humidity.
* **Diagnostic Telemetry:** RSSI link health, battery level, online/offline status badge.
* **Direct Node Interaction:** Trigger LED toggles, reporting intervals, and reboot signals.
* **Instant ACK Display:** Real-time feedback showing command delivery and execution status.
* **Field OTA Upgrade Panel:** Input firmware URL and trigger remote node updates directly from the UI.
* **Offline Demo Mode:** Realistic simulated fallback when disconnected from physical hardware.

---

## Modular Sensor HAL Extension Guide

AgriNetPro uses a Hardware Abstraction Layer allowing new physical sensors to be added in 4 simple steps:

1. **Implement Driver:** Create `driver_<name>.c` implementing the `app_sensor_driver_t` interface (`init`, `read`, `deinit`).
2. **Register Extern:** Add `extern const app_sensor_driver_t sensor_driver_<name>;` to `app_sensor_driver.h`.
3. **Kconfig Binding:** Declare your sensor in `sensor_node/components/app_sensor/Kconfig.projbuild`.
4. **Wire Driver:** Map the selection in `app_sensor.c` and add source file in `CMakeLists.txt`.

### Recommended Sensors for AgriNetPro Applications
* **Poultry Health Monitoring:**
  * Ammonia (NH3) & CO2: `MQ-135` or electrochemical NH3 probes.
  * Lux & Lighting Schedule: `TSL2561` / `BH1750` for circadian egg production optimization.
* **Smart Greenhouses:**
  * Microclimate: `SHT31-D` (High RH tolerance) / `BME280`.
  * Soil Moisture & Salinity: Capacitive Soil Moisture or Modbus RS485 soil sensor.
  * PAR Light Sensor: `VEML7700`.
* **Open Field Agriculture:**
  * Multi-depth soil temperature: `DS18B20` waterproof array.
  * Rainfall / Anemometer: GPIO pulse-counting rain gauge and wind speed meters.

---

## Production Hardening Status

- [x] **Registry Flash Persistence:** Full NVS serialization of devices, intervals, and counters.
- [x] **Bidirectional Command ACKs:** End-to-end execution confirmation from leaf nodes to MQTT.
- [x] **Sensor Node OTA Download Engine:** Background `esp_https_ota` with sleep protection.
- [x] **Dual 1.94 MB App Partitioning:** Elimination of flash overflows for secure Thread binaries.
- [x] **Transport Layer Security:** CoAPS DTLS PSK on port 5684 + MQTTS TLS with ESP CRT bundle.
- [x] **Thread Modem-Sleep & Deep-Sleep:** Retention of operational datasets in NVS across power cycles.
- [x] **Multi-Target CI Automation:** Continuous verification across RCP, Gateway, and Node builds.

---

## License
Apache 2.0 — compatible with Espressif IoT Development Framework and OpenThread.
