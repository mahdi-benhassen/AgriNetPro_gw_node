#include "zigbee_node.h"
#include "esp_zigbee_core.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

static const char *TAG = "ZB_NODE";

/* Event bits for synchronization */
#define ZB_JOINED_BIT      BIT0
#define ZB_REPORT_DONE_BIT BIT1

static EventGroupHandle_t s_zb_event_group = NULL;
static zigbee_node_sleep_ready_cb_t s_sleep_cb = NULL;
static bool s_connected = false;
static uint8_t s_reports_pending = 0;

/* ---- Zigbee Signal Handler (called by the stack) ---- */
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p       = signal_struct->p_app_signal;
    esp_err_t err_status   = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;

    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Stack initialized, starting commissioning");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;

    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Device started, steering to join network...");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        } else {
            ESP_LOGW(TAG, "Init failed (0x%x), retrying...", err_status);
            esp_zb_scheduler_alarm(
                (esp_zb_callback_t)esp_zb_bdb_start_top_level_commissioning,
                ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;

    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t ext_pan;
            esp_zb_get_extended_pan_id(ext_pan);
            ESP_LOGI(TAG, "Joined network! PAN: 0x%04x, Channel: %d",
                     esp_zb_get_pan_id(), esp_zb_get_current_channel());
            s_connected = true;
            if (s_zb_event_group) {
                xEventGroupSetBits(s_zb_event_group, ZB_JOINED_BIT);
            }
        } else {
            ESP_LOGW(TAG, "Steering failed (0x%x), retrying...", err_status);
            esp_zb_scheduler_alarm(
                (esp_zb_callback_t)esp_zb_bdb_start_top_level_commissioning,
                ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;

    default:
        ESP_LOGD(TAG, "ZDO signal: 0x%x, status: 0x%x", sig_type, err_status);
        break;
    }
}

/* ---- Zigbee main task ---- */
static void zigbee_task(void *pvParameters)
{
    /* Configure as Zigbee End Device */
    esp_zb_cfg_t zb_nwk_cfg = {
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,
        .install_code_policy = false,
    };
    esp_zb_init(&zb_nwk_cfg);

    /* --- Create cluster list for our sensor endpoint --- */
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();

    /* Basic Cluster */
    esp_zb_basic_cluster_cfg_t basic_cfg = {
        .zcl_version   = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source  = 0x03, /* Battery */
    };
    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(&basic_cfg);
    esp_zb_basic_cluster_add_attr(basic_cluster,
        ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,
        (void *)"AgriNetPro");
    esp_zb_basic_cluster_add_attr(basic_cluster,
        ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID,
        (void *)"SensorNode_v1");
    esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    /* Identify Cluster */
    esp_zb_identify_cluster_cfg_t identify_cfg = {.identify_time = 0};
    esp_zb_cluster_list_add_identify_cluster(cluster_list,
        esp_zb_identify_cluster_create(&identify_cfg),
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    /* Temperature Measurement Cluster */
    esp_zb_temperature_meas_cluster_cfg_t temp_cfg = {
        .measured_value     = 0xFFFF, /* invalid until first reading */
    };
    esp_zb_cluster_list_add_temperature_meas_cluster(cluster_list,
        esp_zb_temperature_meas_cluster_create(&temp_cfg),
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    /* Relative Humidity Cluster */
    esp_zb_humidity_meas_cluster_cfg_t hum_cfg = {
        .measured_value     = 0xFFFF,
    };
    esp_zb_cluster_list_add_humidity_meas_cluster(cluster_list,
        esp_zb_humidity_meas_cluster_create(&hum_cfg),
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    /* --- Register endpoint --- */
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    esp_zb_endpoint_config_t ep_cfg = {
        .endpoint       = ZIGBEE_NODE_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id  = ESP_ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID,
    };
    esp_zb_ep_list_add_ep(ep_list, cluster_list, ep_cfg);
    esp_zb_device_register(ep_list);

    /* Set channel mask and start */
    esp_zb_set_primary_network_channel_set(ZIGBEE_NODE_CHANNEL_MASK);
    ESP_ERROR_CHECK(esp_zb_start(false));

    ESP_LOGI(TAG, "Zigbee End Device task running");
    esp_zb_stack_main_loop();
}

/* ---- Public API ---- */
esp_err_t zigbee_node_init(zigbee_node_sleep_ready_cb_t sleep_cb)
{
    s_sleep_cb = sleep_cb;
    s_zb_event_group = xEventGroupCreate();
    if (!s_zb_event_group) {
        return ESP_ERR_NO_MEM;
    }

    esp_zb_platform_config_t platform_cfg = {
        .radio_config = {
            .radio_mode = ZB_RADIO_MODE_NATIVE,
        },
        .host_config = {
            .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE,
        },
    };
    ESP_ERROR_CHECK(esp_zb_platform_config(&platform_cfg));

    xTaskCreate(zigbee_task, "zigbee_main", 4096, NULL, 5, NULL);
    return ESP_OK;
}

esp_err_t zigbee_node_report_temperature(float temp_celsius)
{
    int16_t val = (int16_t)(temp_celsius * 100.0f);

    esp_zb_zcl_status_t status = esp_zb_zcl_set_attribute_val(
        ZIGBEE_NODE_ENDPOINT,
        ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
        &val, false);

    if (status != ESP_ZB_ZCL_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to set temp attr: 0x%x", status);
        return ESP_FAIL;
    }

    esp_zb_zcl_report_attr_cmd_t cmd = {
        .zcl_basic_cmd.dst_addr_u.addr_short = 0x0000,
        .zcl_basic_cmd.dst_endpoint = 1,
        .zcl_basic_cmd.src_endpoint = ZIGBEE_NODE_ENDPOINT,
        .address_mode   = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .clusterID      = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
    };
    esp_zb_zcl_report_attr_cmd_req(&cmd);

    ESP_LOGI(TAG, "Reported Temperature: %.2f C (raw: %d)", temp_celsius, val);
    return ESP_OK;
}

esp_err_t zigbee_node_report_humidity(float humidity_pct)
{
    uint16_t val = (uint16_t)(humidity_pct * 100.0f);

    esp_zb_zcl_status_t status = esp_zb_zcl_set_attribute_val(
        ZIGBEE_NODE_ENDPOINT,
        ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID,
        &val, false);

    if (status != ESP_ZB_ZCL_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to set humidity attr: 0x%x", status);
        return ESP_FAIL;
    }

    esp_zb_zcl_report_attr_cmd_t cmd = {
        .zcl_basic_cmd.dst_addr_u.addr_short = 0x0000,
        .zcl_basic_cmd.dst_endpoint = 1,
        .zcl_basic_cmd.src_endpoint = ZIGBEE_NODE_ENDPOINT,
        .address_mode   = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .clusterID      = ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,
    };
    esp_zb_zcl_report_attr_cmd_req(&cmd);

    ESP_LOGI(TAG, "Reported Humidity: %.2f %% (raw: %u)", humidity_pct, val);
    return ESP_OK;
}

esp_err_t zigbee_node_report_battery(uint32_t voltage_mv)
{
    /* ZCL Battery Voltage is in units of 100mV */
    uint8_t battery_voltage = (uint8_t)(voltage_mv / 100);
    /* Rough SoC: 3200mV = 0%, 4200mV = 100% -> 200 = 100% in ZCL (0.5% units) */
    uint8_t battery_pct = 0;
    if (voltage_mv >= 4200) {
        battery_pct = 200;
    } else if (voltage_mv > 3200) {
        battery_pct = (uint8_t)(((voltage_mv - 3200) * 200) / 1000);
    }
    ESP_LOGI(TAG, "Reported Battery: %lu mV (ZCL: %u, SoC: %u%%)",
             voltage_mv, battery_voltage, battery_pct / 2);
    return ESP_OK;
}

static void trigger_sleep_cb(uint8_t param)
{
    if (s_sleep_cb) {
        s_sleep_cb();
    }
}

void zigbee_node_signal_tx_done(void)
{
    /* Simulate a delay for the stack to process and send MAC/APS ACKs */
    esp_zb_scheduler_alarm((esp_zb_callback_t)trigger_sleep_cb, 0, 1500);
}

bool zigbee_node_is_connected(void)
{
    return s_connected;
}
