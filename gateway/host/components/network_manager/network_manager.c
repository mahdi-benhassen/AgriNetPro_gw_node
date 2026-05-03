#include "network_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "NET_MGR";

#define NVS_NAMESPACE       "wifi_creds"
#define NVS_KEY_SSID        "ssid"
#define NVS_KEY_PASS        "pass"
#define BUFFER_FILE_PATH    "/spiffs/buffer.txt"
#define MAX_CRED_LEN        32
#define BUFFER_LINE_MAX     256

static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static bool s_mqtt_connected = false;

static const char *DEFAULT_BROKER_URI = "mqtts://mqtt.agrinet.local:8883";

static void flush_offline_buffer(void);
static esp_err_t load_wifi_credentials(char *ssid, char *password);

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT Connected");
        s_mqtt_connected = true;
        flush_offline_buffer();
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT Disconnected");
        s_mqtt_connected = false;
        break;
    default:
        break;
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Wi-Fi disconnected, reconnecting...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Wi-Fi Connected.");
    }
}

static esp_err_t init_spiffs(void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS for offline buffering...");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "spiffs",
        .max_files = 5,
        .format_if_mount_failed = true,
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK && ret != ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "SPIFFS mounted successfully");
    return ESP_OK;
}

static esp_err_t load_wifi_credentials(char *ssid, char *password)
{
    nvs_handle_t nvs = {0};
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No WiFi credentials in NVS, using defaults");
        return ret;
    }

    size_t len = MAX_CRED_LEN;
    ret = nvs_get_str(nvs, NVS_KEY_SSID, ssid, &len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No SSID in NVS");
        nvs_close(nvs);
        return ret;
    }

    len = MAX_CRED_LEN;
    ret = nvs_get_str(nvs, NVS_KEY_PASS, password, &len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No password in NVS");
        nvs_close(nvs);
        return ret;
    }

    nvs_close(nvs);
    ESP_LOGI(TAG, "WiFi credentials loaded from NVS");
    return ESP_OK;
}

esp_err_t network_manager_set_credentials(const char *ssid, const char *password)
{
    if (!ssid || !password) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs = {0};
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace");
        return ret;
    }

    ret = nvs_set_str(nvs, NVS_KEY_SSID, ssid);
    if (ret != ESP_OK) {
        nvs_close(nvs);
        return ret;
    }

    ret = nvs_set_str(nvs, NVS_KEY_PASS, password);
    if (ret != ESP_OK) {
        nvs_close(nvs);
        return ret;
    }

    ret = nvs_commit(nvs);
    nvs_close(nvs);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi credentials saved to NVS");
    }
    return ret;
}

static void flush_offline_buffer(void)
{
    FILE *f = fopen(BUFFER_FILE_PATH, "r");
    if (!f) {
        ESP_LOGI(TAG, "No offline buffer to flush");
        return;
    }

    char line[BUFFER_LINE_MAX];
    int flushed = 0;

    ESP_LOGI(TAG, "Flushing offline telemetry buffer...");
    while (fgets(line, sizeof(line), f) != NULL) {
        char *delimiter = strchr(line, '|');
        if (!delimiter) {
            continue;
        }
        *delimiter = '\0';
        char *topic = line;
        char *payload = delimiter + 1;

        size_t payload_len = strlen(payload);
        if (payload_len > 0 && payload[payload_len - 1] == '\n') {
            payload[payload_len - 1] = '\0';
        }

        int msg_id = esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 1, 0);
        if (msg_id >= 0) {
            flushed++;
            ESP_LOGD(TAG, "Flushed [%s]: %s (id: %d)", topic, payload, msg_id);
        } else {
            ESP_LOGW(TAG, "Failed to flush telemetry, will retry later");
        }
    }

    fclose(f);

    if (flushed > 0) {
        ESP_LOGI(TAG, "Flushed %d buffered telemetry messages", flushed);
        remove(BUFFER_FILE_PATH);
    }
}

esp_err_t network_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing Network Components...");

    ESP_ERROR_CHECK(init_spiffs());

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    char ssid[MAX_CRED_LEN] = {0};
    char password[MAX_CRED_LEN] = {0};
    if (load_wifi_credentials(ssid, password) != ESP_OK) {
        strncpy(ssid, "AgriNet", sizeof(ssid) - 1);
        strncpy(password, "", sizeof(password) - 1);
    }

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = DEFAULT_BROKER_URI,
        .session.protocol_ver = MQTT_PROTOCOL_V_3_1_1,
    };
    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_mqtt_client);

    return ESP_OK;
}

esp_err_t network_manager_publish_telemetry(const char *topic, const char *payload)
{
    if (!topic || !payload) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_mqtt_connected && s_mqtt_client) {
        int msg_id = esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 1, 0);
        ESP_LOGD(TAG, "MQTT PUB [QoS1] %s: %s (id: %d)", topic, payload, msg_id);
    } else {
        ESP_LOGW(TAG, "MQTT Offline! Buffering telemetry: %s", payload);
        FILE *f = fopen(BUFFER_FILE_PATH, "a");
        if (f) {
            fprintf(f, "%s|%s\n", topic, payload);
            fclose(f);
        } else {
            ESP_LOGE(TAG, "Failed to open buffer file for writing");
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}
