# ESP Thread Sensor Network — Application Layer

A production-grade IoT application stack built **on top of the
[ESP Thread Border Router SDK](https://docs.espressif.com/projects/esp-thread-br/en/latest/)**.

```
┌──────────────────────────────────────────────────────────────────┐
│                     APPLICATION LAYER (this repo)                │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │   Dashboard (React)   REST API   MQTT Bridge   CoAP Server │  │
│  └────────────────────────────────────────────────────────────┘  │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │                    Device Registry                          │  │
│  └────────────────────────────────────────────────────────────┘  │
├──────────────────────────────────────────────────────────────────┤
│            ESP Thread Border Router SDK  (esp-thread-br)         │
│            basic_thread_border_router example                    │
├──────────────────────────────────────────────────────────────────┤
│  ESP-IDF v5.5.4   +   OpenThread 1.4   +   Wi-Fi / Ethernet     │
└──────────────────────────────────────────────────────────────────┘
                              │ Thread (802.15.4)
┌─────────────────────────────▼────────────────────────────────────┐
│        Sensor Node  (ESP32-H2)  — multiple nodes                 │
│    DHT22 / SHT31 → CoAP Client → Sleepy End Device              │
└──────────────────────────────────────────────────────────────────┘
```

---

## Hardware Required

| Role          | Board                          | Chip         |
|---------------|-------------------------------|--------------|
| Border Router | ESP Thread Border Router Board | ESP32-S3 + ESP32-H2 (RCP) |
| Sensor Node   | ESP32-H2 DevKit               | ESP32-H2     |
| Sensor        | DHT22 **or** SHT31-D          | —            |

---

## Repository Layout

```
esp-thread-app/
├── common/
│   ├── app_protocol.h      ← Shared binary payload & CoAP/MQTT constants
│   └── app_config.h        ← Compile-time defaults
│
├── sensor_node/            ← Firmware for ESP32-H2 end nodes
│   ├── CMakeLists.txt
│   ├── Kconfig.projbuild
│   ├── sdkconfig.defaults
│   ├── partitions.csv
│   ├── main/
│   │   └── app_node_main.c ← Entry point: Thread attach → read → CoAP send
│   └── components/
│       ├── app_sensor/     ← DHT22 (bit-bang) + SHT31 (I2C) drivers
│       ├── app_coap_client/← CoAP POST telemetry / GET commands
│       └── app_sleep/      ← Sleepy End Device + deep-sleep support
│
├── border_router/          ← Firmware for ESP32-S3 Border Router
│   ├── CMakeLists.txt
│   ├── Kconfig.projbuild
│   ├── sdkconfig.defaults
│   ├── partitions.csv
│   ├── main/
│   │   └── app_br_main.c  ← Entry point: Wi-Fi → Thread BR → services
│   └── components/
│       ├── app_device_registry/ ← Thread-safe node tracking + cmd queue
│       ├── app_coap_server/     ← CoAP endpoints on Thread mesh interface
│       ├── app_mqtt_bridge/     ← Publishes JSON telemetry to MQTT broker
│       └── app_rest_api/        ← HTTP REST API (GET nodes, POST commands)
│
└── dashboard/
    └── ThreadMonitorDashboard.jsx ← React live dashboard
```

---

## 1  Environment Setup

Follow the [esp-thread-br Build and Run guide](https://docs.espressif.com/projects/esp-thread-br/en/latest/dev-guide/build_and_run.html):

```bash
# 1. Clone ESP-IDF (required version)
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf && git checkout v5.5.4
git submodule update --init --depth 1
./install.sh && . ./export.sh
cd ..

# 2. Clone esp-thread-br SDK
git clone --recursive https://github.com/espressif/esp-thread-br.git
export ESP_THREAD_BR_PATH=$(pwd)/esp-thread-br

# 3. Clone this application layer
git clone <this-repo> esp-thread-app
```

---

## 2  Build the RCP Firmware (ESP32-H2 on the Border Router board)

The RCP (Radio Co-Processor) runs on the built-in ESP32-H2 of the Border
Router board.  It is flashed automatically on first boot by the host firmware.

```bash
cd $IDF_PATH/examples/openthread/ot_rcp
idf.py set-target esp32h2
idf.py build
# No flash needed — the BR host copies this image automatically
```

---

## 3  Build & Flash the Border Router

```bash
cd esp-thread-app/border_router

# Edit sdkconfig.defaults first:
#   CONFIG_EXAMPLE_WIFI_SSID    → your Wi-Fi SSID
#   CONFIG_EXAMPLE_WIFI_PASSWORD → your Wi-Fi password
#   CONFIG_MQTT_BROKER_URI      → mqtt://192.168.x.x:1883

idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### Manual Thread network setup (first boot)

```
# In the monitor console:
ot wifi connect -s <ssid> -p <password>
ot dataset init new
ot dataset commit active
ot ifconfig up
ot thread start

# Record the dataset for sensor nodes:
ot dataset active -x
```

---

## 4  Build & Flash Sensor Nodes (one per ESP32-H2 DevKit)

```bash
cd esp-thread-app/sensor_node

# Wire the DHT22:
#   DHT22 VCC  → 3V3
#   DHT22 DATA → GPIO8  (with 10 kΩ pull-up to 3V3)
#   DHT22 GND  → GND

# Edit Kconfig / sdkconfig.defaults:
#   CONFIG_APP_NODE_LABEL → unique name ("Living Room", "Bedroom", ...)
#   CONFIG_OPENTHREAD_NETWORK_MASTERKEY → must match border router

idf.py set-target esp32h2
idf.py build
idf.py -p /dev/ttyUSB1 flash monitor

# Paste the dataset from the border router:
ot dataset set active <hex-dataset>
ot dataset commit active
ot ifconfig up
ot thread start
```

The node will:
1. Join the Thread network as a Sleepy End Device
2. Register itself with the Border Router via CoAP POST `/sensor/register`
3. Read the sensor every 30 s and POST `/sensor/data`
4. Poll for downlink commands via CoAP GET `/cmd`

---

## 5  REST API Reference

Base URL: `http://<border-router-ip>:8080`

| Method | Endpoint                        | Description                        |
|--------|---------------------------------|------------------------------------|
| GET    | `/api/v1/nodes`                 | JSON array of all registered nodes |
| GET    | `/api/v1/nodes/<eui64>`         | Single node details + last reading |
| POST   | `/api/v1/nodes/<eui64>/cmd`     | Queue a downlink command           |
| GET    | `/api/v1/status`                | BR uptime + node count             |

### POST `/api/v1/nodes/<eui64>/cmd` body

```json
{
  "cmd":   1,              // CMD_SET_INTERVAL
  "param": 60,             // new interval in seconds
  "data":  null            // optional string payload (e.g. OTA URL)
}
```

| cmd | Name            | Description                         |
|-----|-----------------|-------------------------------------|
| 1   | SET_INTERVAL    | Change reporting interval (param=s) |
| 2   | REBOOT          | Reboot the node                     |
| 3   | OTA_START       | Start OTA (data = firmware URL)     |
| 4   | LED_ON          | Turn LED on                         |
| 5   | LED_OFF         | Turn LED off                        |

---

## 6  MQTT Topics

All topics follow the pattern: `esp/thread/<network_id>/<eui64>/<subtopic>`

| Direction  | Subtopic    | Payload        | Description                   |
|------------|-------------|----------------|-------------------------------|
| ↑ Publish  | `telemetry` | JSON (see below)| Sensor readings every report  |
| ↑ Publish  | `status`    | `{"status":"online"}` | Node connect/disconnect|
| ↓ Subscribe| `cmd/down`  | JSON           | Downlink command from cloud   |
| ↓ Subscribe| `ota`       | JSON           | OTA firmware push             |

### Telemetry payload example

```json
{
  "network_id":    "HomeNet01",
  "label":         "Living Room",
  "eui64":         "48F17EC300A1B2C3",
  "temperature_c": 22.4,
  "humidity_pct":  58.1,
  "battery_mv":    0,
  "rssi_dbm":      -62,
  "uptime_s":      86400,
  "seq":           1482,
  "battery_low":   false,
  "ts":            1716307200
}
```

---

## 7  Dashboard

Open `dashboard/ThreadMonitorDashboard.jsx` in the Claude artifact viewer
(or copy into a React project).  Set the API URL in the top-right input field
to your Border Router's IP address.  The dashboard:

- Polls `/api/v1/nodes` every 5 seconds
- Displays live temperature / humidity with sparkline history
- Shows RSSI, battery voltage, firmware version, and online/offline status
- Lets you send commands (LED, reboot, set interval) directly from the UI
- Falls back to animated demo data when no real API is reachable

---

## 8  Integrating New Sensors (Modular HAL)

The sensor node firmware uses a Hardware Abstraction Layer (HAL) to support a modular, plug-and-play sensor architecture. The main application (`app_node_main.c`) is completely decoupled from the specific sensor hardware. The selection is done at build time using `menuconfig`.

### Step-by-Step Guide to Adding a New Sensor

**1. Create the Driver File**
Create a new C file in `sensor_node/components/app_sensor/driver_my_sensor.c` that implements the `app_sensor_driver_t` interface:

```c
#include "app_sensor_driver.h"

static esp_err_t my_sensor_init(void) {
    // Initialize I2C/SPI/ADC etc.
    return ESP_OK;
}

static esp_err_t my_sensor_read(app_sensor_reading_t *out) {
    // Read from the sensor hardware
    out->temperature_c = 25.0f;
    out->humidity_pct = 50.0f;
    out->valid = true;
    return ESP_OK;
}

static esp_err_t my_sensor_deinit(void) {
    // Clean up
    return ESP_OK;
}

// Export the driver struct
const app_sensor_driver_t sensor_driver_my_sensor = {
    .init = my_sensor_init,
    .read = my_sensor_read,
    .deinit = my_sensor_deinit
};
```

**2. Update the HAL Registry**
In `sensor_node/components/app_sensor/include/app_sensor_driver.h`, declare your new driver:
```c
extern const app_sensor_driver_t sensor_driver_my_sensor;
```

**3. Add Kconfig Options**
Edit `sensor_node/components/app_sensor/Kconfig.projbuild` to add your sensor to the choice menu and define any configuration values:
```kconfig
config APP_SENSOR_TYPE_MY_SENSOR
    bool "My Custom Sensor (I2C)"

config APP_SENSOR_MY_SENSOR_I2C_ADDR
    hex "I2C Address"
    depends on APP_SENSOR_TYPE_MY_SENSOR
    default 0x44
```

**4. Wire the Build-Time Selection**
In `sensor_node/components/app_sensor/app_sensor.c`, map the Kconfig selection to your driver:
```c
static const app_sensor_driver_t *s_driver = 
#if defined(CONFIG_APP_SENSOR_TYPE_DHT22)
    &sensor_driver_dht22;
#elif defined(CONFIG_APP_SENSOR_TYPE_SHT31)
    &sensor_driver_sht31;
#elif defined(CONFIG_APP_SENSOR_TYPE_MY_SENSOR)
    &sensor_driver_my_sensor;
#else
    NULL;
#endif
```

**5. Include the File in CMake**
Finally, add your new file to `sensor_node/components/app_sensor/CMakeLists.txt` using the Kconfig condition:
```cmake
set(srcs "app_sensor.c")

if(CONFIG_APP_SENSOR_TYPE_DHT22)
    list(APPEND srcs "driver_dht22.c")
elseif(CONFIG_APP_SENSOR_TYPE_SHT31)
    list(APPEND srcs "driver_sht31.c")
elseif(CONFIG_APP_SENSOR_TYPE_MY_SENSOR)
    list(APPEND srcs "driver_my_sensor.c")
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "include"
    REQUIRES driver esp_adc log freertos
)
```

---

## 9  Recommended Sensors for AgriNetPro

To cover the specific needs of smart poultry systems, smart agriculture, and greenhouses, the following sensors are recommended for integration:

### 1. Smart Greenhouses
*   **Air Temperature & Humidity:** `SHT31-D` or `BME280` (High precision, reliable in high humidity).
*   **Soil Moisture & Temperature:** `SHT-10` (Robust, waterproof housing) or `Capacitive Soil Moisture Sensors` (Corrosion-resistant).
*   **CO2 Levels:** `SCD30` or `MH-Z19` (Essential for monitoring CO2 enrichment in greenhouses).
*   **Light Intensity (PAR):** `VEML7700` or `BH1750` (Optimizing grow lights or shading screens).

### 2. Smart Poultry Systems
*   **Air Quality (Ammonia & CO2):** `MQ-135` or specialized NH3 electrochemical sensors (Crucial for poultry health and ventilation control).
*   **Ambient Temperature & Humidity:** `DHT22` or `SHT31-D` (Monitoring heat stress index).
*   **Luminosity:** `TSL2561` (Monitoring daylight and artificial lighting schedules for egg production).

### 3. Open Field Smart Agriculture
*   **Soil Parameters:** `DS18B20` (Soil temperature) and `SMT50` (Volumetric water content).
*   **Microclimate Weather Stations:** Integration of Anemometers (Wind Speed) and Tipping Bucket Rain Gauges via GPIO pulse counting.
*   **Leaf Wetness:** Resistive/capacitive leaf wetness grids (Predicting fungal diseases).

---

## 10  Customising the Sensor Protocol

All shared constants live in `common/app_protocol.h`.  If your new sensor returns fields beyond just temperature and humidity, you will need to extend the protocol:

1. Add an entry to `app_sensor_type_t` enum
2. Extend `app_sensor_payload_t` if needed (bump `APP_PROTO_VERSION`)
3. Update `app_device_registry_update()` in the border router to store the new fields
4. Update the MQTT bridge JSON builder (`app_mqtt_bridge.c`) to publish the new fields

---

## 11  Production Hardening Checklist

- [ ] Enable DTLS on CoAP (set `CONFIG_COAP_MBEDTLS_PSK=y`, provision PSK)
- [ ] Use TLS MQTT (`mqtts://` + server certificate)
- [ ] Enable NVS encryption (`CONFIG_NVS_ENCRYPTION=y`)
- [ ] Set a real Thread Network Master Key (not the default)
- [ ] Enable OTA signing (`CONFIG_SECURE_BOOT=y`)
- [ ] Set `CONFIG_LOG_DEFAULT_LEVEL_WARN` for production builds
- [ ] Add watchdog timer resets in long-running tasks
- [ ] Configure SNTP for accurate timestamps in telemetry

---

## License

Apache 2.0 — same as the ESP Thread Border Router SDK.
