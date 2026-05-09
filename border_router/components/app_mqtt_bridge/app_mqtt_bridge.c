/**
 * @file app_mqtt_bridge.c
 * @brief MQTT bridge: publishes Thread sensor telemetry to a cloud broker
 *        and subscribes to downlink command topics.
 *
 * Topic format (publish):
 *   esp/thread/<network_id>/<eui64_hex>/telemetry
 *   esp/thread/<network_id>/<eui64_hex>/status
 *
 * Topic format (subscribe):
 *   esp/thread/<network_id>/+/cmd/down     → downlink commands
 *   esp/thread/<network_id>/+/ota          → OTA URL push
 *
 * Payload format: JSON (human-readable, easy to parse in dashboards).
 */

#include "app_mqtt_bridge.h"
#include "app_config.h"
#include "app_protocol.h"
#include "app_device_registry.h"
#include "esp_log.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG = TAG_MQTT;

static esp_mqtt_client_handle_t s_client = NULL;
static bool                      s_connected = false;

/* ─── Build topic string ─────────────────────────────────────────────────── */
static int build_topic(char *buf, size_t len,
                        uint64_t eui64, const char *subtopic)
{
    return snprintf(buf, len, "%s/%s/%016llX/%s",
                    MQTT_TOPIC_PREFIX, APP_NETWORK_ID,
                    (unsigned long long)eui64, subtopic);
}

/* ─── Publish telemetry JSON ─────────────────────────────────────────────── */
static void publish_telemetry(const app_device_entry_t *dev,
                               const app_sensor_payload_t *p)
{
    if (!s_connected || !s_client) return;

    char topic[MQTT_TOPIC_MAX_LEN];
    build_topic(topic, sizeof(topic), p->eui64, MQTT_SUBTOPIC_TELEMETRY);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "network_id", APP_NETWORK_ID);
    cJSON_AddStringToObject(root, "label",      dev->label);
    char eui_str[17];
    snprintf(eui_str, sizeof(eui_str), "%016llX", (unsigned long long)p->eui64);
    cJSON_AddStringToObject(root, "eui64",      eui_str);
    cJSON_AddNumberToObject(root, "temperature_c",
                            TEMP_RAW_TO_FLOAT(p->temperature_c));
    cJSON_AddNumberToObject(root, "humidity_pct",
                            HUM_RAW_TO_FLOAT(p->humidity_pct));
    cJSON_AddNumberToObject(root, "battery_mv",  p->battery_mv);
    cJSON_AddNumberToObject(root, "rssi_dbm",    p->rssi_dbm);
    cJSON_AddNumberToObject(root, "uptime_s",    p->uptime_s);
    cJSON_AddNumberToObject(root, "seq",         p->seq_num);
    cJSON_AddBoolToObject(root,   "battery_low",
                          (p->node_flags & NODE_FLAG_BATTERY_LOW) != 0);
    cJSON_AddNumberToObject(root, "ts",          (double)time(NULL));

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json) {
        esp_mqtt_client_publish(s_client, topic, json, 0,
                                MQTT_QOS, MQTT_RETAIN);
        ESP_LOGD(TAG, "MQTT ↑ %s: %s", topic, json);
        free(json);
    }
}

/* ─── Publish node status (online / offline) ─────────────────────────────── */
static void publish_status(uint64_t eui64, bool online)
{
    if (!s_connected || !s_client) return;

    char topic[MQTT_TOPIC_MAX_LEN];
    build_topic(topic, sizeof(topic), eui64, MQTT_SUBTOPIC_STATUS);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", online ? "online" : "offline");
    cJSON_AddNumberToObject(root, "ts",     (double)time(NULL));
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json) {
        /* Status published with retain=1 so a late subscriber sees current state */
        esp_mqtt_client_publish(s_client, topic, json, 0, MQTT_QOS, 1);
        free(json);
    }
}

/* ─── Handle incoming downlink command ───────────────────────────────────── */
static void handle_downlink(const char *topic, const char *payload)
{
    ESP_LOGI(TAG, "MQTT ↓ %s: %s", topic, payload);

    /*
     * Parse EUI-64 from topic:
     *   esp/thread/<netid>/<eui64>/cmd/down
     *   index:  0    1       2       3    4   5
     */
    char topic_copy[MQTT_TOPIC_MAX_LEN];
    strncpy(topic_copy, topic, sizeof(topic_copy) - 1);

    char *tok, *saveptr;
    int  idx = 0;
    uint64_t eui64 = 0;
    tok = strtok_r(topic_copy, "/", &saveptr);
    while (tok) {
        if (idx == 3) {
            eui64 = strtoull(tok, NULL, 16);
            break;
        }
        idx++;
        tok = strtok_r(NULL, "/", &saveptr);
    }
    if (!eui64) return;

    cJSON *root = cJSON_Parse(payload);
    if (!root) {
        ESP_LOGW(TAG, "Failed to parse command JSON");
        return;
    }

    cJSON *cmd_type_j = cJSON_GetObjectItem(root, "cmd");
    cJSON *param_j    = cJSON_GetObjectItem(root, "param");
    cJSON *data_j     = cJSON_GetObjectItem(root, "data");

    if (!cJSON_IsNumber(cmd_type_j)) {
        cJSON_Delete(root);
        return;
    }

    app_cmd_type_t cmd_type = (app_cmd_type_t)cmd_type_j->valueint;
    uint16_t param = param_j ? (uint16_t)param_j->valueint : 0;
    const char *data = data_j ? data_j->valuestring : NULL;

    app_device_registry_queue_cmd(eui64, cmd_type, param, data);
    cJSON_Delete(root);
}

/* ─── MQTT event handler ─────────────────────────────────────────────────── */
static void mqtt_event_handler(void *arg,
                                esp_event_base_t base,
                                int32_t event_id,
                                void *event_data)
{
    esp_mqtt_event_handle_t ev = (esp_mqtt_event_handle_t)event_data;

    switch (event_id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "MQTT connected to %s", MQTT_BROKER_URI);

        /* Subscribe to downlink command topics */
        {
            char sub_topic[MQTT_TOPIC_MAX_LEN];
            snprintf(sub_topic, sizeof(sub_topic),
                     "%s/%s/+/%s",
                     MQTT_TOPIC_PREFIX, APP_NETWORK_ID, MQTT_SUBTOPIC_CMD_DOWN);
            esp_mqtt_client_subscribe(s_client, sub_topic, MQTT_QOS);
            ESP_LOGI(TAG, "Subscribed: %s", sub_topic);

            snprintf(sub_topic, sizeof(sub_topic),
                     "%s/%s/+/%s",
                     MQTT_TOPIC_PREFIX, APP_NETWORK_ID, MQTT_SUBTOPIC_OTA);
            esp_mqtt_client_subscribe(s_client, sub_topic, MQTT_QOS);
        }

        /* Publish online status for all registered devices */
        app_device_registry_foreach(
            [](const app_device_entry_t *dev, void *ud) {
                publish_status(dev->eui64, dev->online);
            }, NULL);
        break;

    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "MQTT disconnected — will auto-reconnect");
        break;

    case MQTT_EVENT_DATA:
        if (ev->topic && ev->data) {
            char topic[MQTT_TOPIC_MAX_LEN] = {0};
            char data[MQTT_PAYLOAD_MAX_LEN] = {0};
            strncpy(topic, ev->topic,   MIN(ev->topic_len,   sizeof(topic)-1));
            strncpy(data,  ev->data,    MIN(ev->data_len,    sizeof(data)-1));

            if (strstr(topic, MQTT_SUBTOPIC_CMD_DOWN) ||
                strstr(topic, MQTT_SUBTOPIC_OTA)) {
                handle_downlink(topic, data);
            }
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error");
        break;

    default:
        break;
    }
}

/* ─── Telemetry callback (registered with device registry) ──────────────── */
static void on_telemetry(const app_device_entry_t *dev,
                          const app_sensor_payload_t *payload)
{
    publish_telemetry(dev, payload);
}

/* ─── Public API ─────────────────────────────────────────────────────────── */

esp_err_t app_mqtt_bridge_start(void)
{
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri          = MQTT_BROKER_URI,
        .credentials.username        = MQTT_USERNAME,
        .credentials.authentication.password = MQTT_PASSWORD,
        .session.keepalive           = MQTT_KEEPALIVE_S,
        .network.reconnect_timeout_ms = 5000,
    };

    s_client = esp_mqtt_client_init(&cfg);
    if (!s_client) {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(s_client,
                                                   ESP_EVENT_ANY_ID,
                                                   mqtt_event_handler,
                                                   NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(s_client));

    /* Hook into device registry to publish on every new reading */
    app_device_registry_set_telemetry_cb(on_telemetry);

    ESP_LOGI(TAG, "MQTT bridge started → %s", MQTT_BROKER_URI);
    return ESP_OK;
}

void app_mqtt_bridge_publish_status(uint64_t eui64, bool online)
{
    publish_status(eui64, online);
}
