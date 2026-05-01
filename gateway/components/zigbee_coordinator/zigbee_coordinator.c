#include "zigbee_coordinator.h"
#include "esp_zigbee_core.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "ZB_COORD";

static zigbee_coord_data_cb_t s_data_cb = NULL;

/* ---- ZCL Action Handler ---- */
static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id,
                                   const void *message)
{
    switch (callback_id) {
    case ESP_ZB_CORE_REPORT_ATTR_CB_ID: {
        esp_zb_zcl_report_attr_message_t *report =
            (esp_zb_zcl_report_attr_message_t *)message;

        if (!report || report->status != ESP_ZB_ZCL_STATUS_SUCCESS) {
            ESP_LOGW(TAG, "Received report with error status");
            return ESP_OK;
        }

        ESP_LOGI(TAG, "Report from 0x%04x EP%d, Cluster 0x%04x, Attr 0x%04x",
                 report->src_address.u.short_addr,
                 report->src_endpoint,
                 report->cluster,
                 report->attribute.id);

        zigbee_coord_node_data_t node_data = {
            .short_addr   = report->src_address.u.short_addr,
            .src_endpoint = report->src_endpoint,
            .temperature  = NAN,
            .humidity     = NAN,
            .battery_mv   = 0,
        };

        /* Parse temperature report */
        if (report->cluster == ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT &&
            report->attribute.id == ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID) {
            int16_t raw = *(int16_t *)report->attribute.data.value;
            node_data.temperature = (float)raw / 100.0f;
            ESP_LOGI(TAG, "  -> Temperature: %.2f C", node_data.temperature);
        }

        /* Parse humidity report */
        if (report->cluster == ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT &&
            report->attribute.id == ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID) {
            uint16_t raw = *(uint16_t *)report->attribute.data.value;
            node_data.humidity = (float)raw / 100.0f;
            ESP_LOGI(TAG, "  -> Humidity: %.2f %%", node_data.humidity);
        }

        /* Dispatch to application callback */
        if (s_data_cb) {
            s_data_cb(&node_data);
        }
        break;
    }

    case ESP_ZB_CORE_CMD_DEFAULT_RESP_CB_ID:
        ESP_LOGD(TAG, "Default response received");
        break;

    default:
        ESP_LOGD(TAG, "Unhandled ZCL action: 0x%x", callback_id);
        break;
    }
    return ESP_OK;
}

/* ---- Zigbee Signal Handler ---- */
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p       = signal_struct->p_app_signal;
    esp_err_t err_status   = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;

    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Zigbee stack initialized");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;

    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Forming Zigbee network...");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
        } else {
            ESP_LOGE(TAG, "Coordinator init failed: 0x%x", err_status);
        }
        break;

    case ESP_ZB_BDB_SIGNAL_FORMATION:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t ext_pan;
            esp_zb_get_extended_pan_id(ext_pan);
            ESP_LOGI(TAG, "Network formed! PAN: 0x%04x, Channel: %d",
                     esp_zb_get_pan_id(), esp_zb_get_current_channel());
            /* Open network for joining */
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        } else {
            ESP_LOGW(TAG, "Network formation failed: 0x%x, retrying...", err_status);
            esp_zb_scheduler_alarm(
                (esp_zb_callback_t)esp_zb_bdb_start_top_level_commissioning,
                ESP_ZB_BDB_MODE_NETWORK_FORMATION, 1000);
        }
        break;

    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Network steering active, accepting joins...");
        }
        break;

    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE: {
        esp_zb_zdo_signal_device_annce_params_t *dev =
            (esp_zb_zdo_signal_device_annce_params_t *)
            esp_zb_app_signal_get_params(signal_struct->p_app_signal);
        ESP_LOGI(TAG, "New device joined! Short addr: 0x%04x", dev->device_short_addr);
        break;
    }

    default:
        ESP_LOGD(TAG, "ZDO signal: 0x%x, status: 0x%x", sig_type, err_status);
        break;
    }
}

/* ---- Zigbee Coordinator Task ---- */
static void zigbee_coord_task(void *pvParameters)
{
    /* Configure as Zigbee Coordinator */
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZC_CONFIG();
    esp_zb_init(&zb_nwk_cfg);

    /* Create minimal coordinator endpoint */
    esp_zb_cluster_list_t *cluster_list = esp_zb_cluster_list_create();

    /* Basic Cluster */
    esp_zb_basic_cluster_cfg_t basic_cfg = {
        .zcl_version  = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = 0x01, /* Mains powered */
    };
    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(&basic_cfg);
    esp_zb_basic_cluster_add_attr(basic_cluster,
        ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, (void *)"AgriNetPro");
    esp_zb_basic_cluster_add_attr(basic_cluster,
        ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, (void *)"Gateway_v1");
    esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    /* Identify Cluster */
    esp_zb_identify_cluster_cfg_t identify_cfg = {.identify_time = 0};
    esp_zb_cluster_list_add_identify_cluster(cluster_list,
        esp_zb_identify_cluster_create(&identify_cfg),
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    /* Register endpoint */
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    esp_zb_endpoint_config_t ep_cfg = {
        .endpoint       = ZIGBEE_COORD_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id  = ESP_ZB_HA_COMBINED_INTERFACE_DEVICE_ID,
    };
    esp_zb_ep_list_add_ep(ep_list, cluster_list, ep_cfg);
    esp_zb_device_register(ep_list);

    /* Register the attribute report callback */
    esp_zb_core_action_handler_register(zb_action_handler);

    /* Set channel mask and start */
    esp_zb_set_primary_network_channel_set(ZIGBEE_COORD_CHANNEL_MASK);
    ESP_ERROR_CHECK(esp_zb_start(false));

    ESP_LOGI(TAG, "Zigbee Coordinator task running");
    esp_zb_stack_main_loop();
}

/* ---- Public API ---- */
esp_err_t zigbee_coordinator_init(zigbee_coord_data_cb_t data_cb)
{
    s_data_cb = data_cb;

    esp_zb_platform_config_t platform_cfg = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config  = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(esp_zb_platform_config(&platform_cfg));

    xTaskCreate(zigbee_coord_task, "zigbee_coord", 4096, NULL, 5, NULL);
    return ESP_OK;
}
