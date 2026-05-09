/**
 * @file app_rest_api.c
 * @brief HTTP/1.1 REST API served by the ESP Thread Border Router.
 *
 * Endpoints:
 *   GET  /api/v1/nodes           → JSON array of all registered nodes
 *   GET  /api/v1/nodes/<eui64>   → JSON object for a single node
 *   POST /api/v1/nodes/<eui64>/cmd → Queue a downlink command
 *   GET  /api/v1/status          → BR uptime + network stats
 *
 * Uses esp_http_server (httpd) from ESP-IDF.
 */

#include "app_rest_api.h"
#include "app_config.h"
#include "app_device_registry.h"
#include "app_protocol.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>

/* Helper stringification for version */
#define STR(x)  STR2(x)
#define STR2(x) #x
#define APP_FW_MAJOR_STR STR(APP_FW_MAJOR)
#define APP_FW_MINOR_STR STR(APP_FW_MINOR)

static const char *TAG = TAG_REST;

/* ─── JSON builders ──────────────────────────────────────────────────────── */

static cJSON *device_to_json(const app_device_entry_t *dev)
{
    cJSON *obj = cJSON_CreateObject();
    char eui_str[17];
    snprintf(eui_str, sizeof(eui_str), "%016llX",
             (unsigned long long)dev->eui64);

    cJSON_AddStringToObject(obj, "eui64",       eui_str);
    cJSON_AddStringToObject(obj, "label",        dev->label);
    cJSON_AddStringToObject(obj, "ipv6",         dev->ipv6_addr);
    cJSON_AddBoolToObject(  obj, "online",       dev->online);
    cJSON_AddNumberToObject(obj, "last_seen",    (double)dev->last_seen);
    cJSON_AddNumberToObject(obj, "fw_version",
                            dev->fw_major * 100 + dev->fw_minor);
    cJSON_AddNumberToObject(obj, "report_interval_s",
                            dev->report_interval_s);
    cJSON_AddNumberToObject(obj, "total_reports", dev->total_reports);

    /* Latest reading */
    cJSON *r = cJSON_AddObjectToObject(obj, "reading");
    cJSON_AddNumberToObject(r, "temperature_c",
        TEMP_RAW_TO_FLOAT(dev->last_reading.temperature_c));
    cJSON_AddNumberToObject(r, "humidity_pct",
        HUM_RAW_TO_FLOAT(dev->last_reading.humidity_pct));
    cJSON_AddNumberToObject(r, "battery_mv", dev->last_reading.battery_mv);
    cJSON_AddNumberToObject(r, "rssi_dbm",   dev->last_reading.rssi_dbm);
    cJSON_AddBoolToObject(r,   "battery_low",
        (dev->last_reading.node_flags & NODE_FLAG_BATTERY_LOW) != 0);

    return obj;
}

/* ─── GET /api/v1/nodes ──────────────────────────────────────────────────── */

typedef struct { cJSON *arr; } foreach_ctx_t;
static void collect_node_json(const app_device_entry_t *dev, void *ud)
{
    foreach_ctx_t *ctx = (foreach_ctx_t *)ud;
    cJSON_AddItemToArray(ctx->arr, device_to_json(dev));
}

static esp_err_t handler_get_nodes(httpd_req_t *req)
{
    cJSON *arr = cJSON_CreateArray();
    foreach_ctx_t ctx = { .arr = arr };
    app_device_registry_foreach(collect_node_json, &ctx);

    char *json = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, json ? json : "[]");
    free(json);
    return ESP_OK;
}

/* ─── GET /api/v1/nodes/<eui64> ──────────────────────────────────────────── */

static esp_err_t handler_get_node(httpd_req_t *req)
{
    /* Extract EUI-64 from URI: /api/v1/nodes/<eui64> */
    const char *uri   = req->uri;
    const char *start = strrchr(uri, '/');
    if (!start || strlen(start) < 2) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing EUI-64");
        return ESP_FAIL;
    }
    start++;   /* skip '/' */
    uint64_t eui64 = strtoull(start, NULL, 16);

    app_device_entry_t dev;
    if (app_device_registry_get(eui64, &dev) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Node not found");
        return ESP_FAIL;
    }

    cJSON *obj  = device_to_json(&dev);
    char  *json = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, json ? json : "{}");
    free(json);
    return ESP_OK;
}

/* ─── POST /api/v1/nodes/<eui64>/cmd ────────────────────────────────────── */

static esp_err_t handler_post_cmd(httpd_req_t *req)
{
    /* Parse EUI-64: second-to-last path segment */
    char uri_copy[128];
    strncpy(uri_copy, req->uri, sizeof(uri_copy) - 1);
    /* Remove trailing /cmd */
    char *slash = strrchr(uri_copy, '/');
    if (slash) *slash = '\0';
    slash = strrchr(uri_copy, '/');
    uint64_t eui64 = slash ? strtoull(slash + 1, NULL, 16) : 0;

    if (!eui64) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid EUI-64");
        return ESP_FAIL;
    }

    /* Read body */
    char body[256] = {0};
    int  body_len  = httpd_req_recv(req, body, sizeof(body) - 1);
    if (body_len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(body);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *cmd_j   = cJSON_GetObjectItem(root, "cmd");
    cJSON *param_j = cJSON_GetObjectItem(root, "param");
    cJSON *data_j  = cJSON_GetObjectItem(root, "data");

    app_cmd_type_t cmd_type = (app_cmd_type_t)
        (cJSON_IsNumber(cmd_j) ? cmd_j->valueint : 0);
    uint16_t    param = (uint16_t)(cJSON_IsNumber(param_j) ? param_j->valueint : 0);
    const char *data  = cJSON_IsString(data_j) ? data_j->valuestring : NULL;

    cJSON_Delete(root);

    esp_err_t ret = app_device_registry_queue_cmd(eui64, cmd_type, param, data);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    if (ret == ESP_OK) {
        httpd_resp_sendstr(req, "{\"status\":\"queued\"}");
    } else {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Node not found");
    }
    return ret;
}

/* ─── GET /api/v1/status ─────────────────────────────────────────────────── */

static esp_err_t handler_get_status(httpd_req_t *req)
{
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "network_id",    APP_NETWORK_ID);
    cJSON_AddNumberToObject(obj, "uptime_s",
                            (double)(esp_timer_get_time() / 1000000ULL));
    cJSON_AddNumberToObject(obj, "node_count",
                            app_device_registry_count());
    cJSON_AddStringToObject(obj, "fw_version",
                            "v" APP_FW_MAJOR_STR "." APP_FW_MINOR_STR);

    char *json = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, json ? json : "{}");
    free(json);
    return ESP_OK;
}



/* ─── Public API ─────────────────────────────────────────────────────────── */

esp_err_t app_rest_api_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port          = REST_API_PORT;
    cfg.max_open_sockets     = REST_API_MAX_CONNECTIONS;
    cfg.uri_match_fn         = httpd_uri_match_wildcard;

    httpd_handle_t server;
    ESP_RETURN_ON_ERROR(httpd_start(&server, &cfg), TAG, "httpd_start failed");

    /* Register URI handlers */
    static const httpd_uri_t uris[] = {
        { .uri = "/api/v1/nodes",
          .method = HTTP_GET, .handler = handler_get_nodes },
        { .uri = "/api/v1/nodes/*",
          .method = HTTP_GET, .handler = handler_get_node  },
        { .uri = "/api/v1/nodes/*/cmd",
          .method = HTTP_POST, .handler = handler_post_cmd },
        { .uri = "/api/v1/status",
          .method = HTTP_GET, .handler = handler_get_status },
    };

    for (size_t i = 0; i < sizeof(uris)/sizeof(uris[0]); i++) {
        httpd_register_uri_handler(server, &uris[i]);
    }

    ESP_LOGI(TAG, "REST API listening on port %d", REST_API_PORT);
    return ESP_OK;
}
