/**
 * @file app_coap_client.c
 * @brief CoAP client implementation using libcoap (bundled with ESP-IDF).
 *
 * This module runs its CoAP I/O on a dedicated FreeRTOS task so that
 * retransmissions and acknowledgements do not block the sensor task.
 *
 * Thread safety: app_coap_client_send() and app_coap_client_poll_cmd()
 * are protected by a mutex and safe to call from any task.
 */

#include "app_coap_client.h"
#include "app_config.h"
#include "app_protocol.h"
#include "esp_log.h"
#include "esp_openthread.h"
#include "esp_openthread_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "coap3/coap.h"
#include <openthread/thread.h>
#include <openthread/ip6.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

static const char *TAG = TAG_COAP;

/* ─── State ───────────────────────────────────────────────────────────────── */
static coap_context_t  *s_ctx      = NULL;
static SemaphoreHandle_t s_mutex   = NULL;
static uint16_t          s_seq_num = 0;

/* ─── Internal: resolve BR address ───────────────────────────────────────────
 * In a real deployment the BR registers itself via SRP with service name
 * "_coap._udp.local".  Here we fall back to the configured mesh-local EID.
 * ────────────────────────────────────────────────────────────────────────── */
static bool resolve_br_address(coap_address_t *addr)
{
    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&addr->addr;
    memset(addr, 0, sizeof(*addr));
    addr->size = sizeof(struct sockaddr_in6);
    sin6->sin6_family = AF_INET6;
    sin6->sin6_port   = htons(BR_COAP_PORT);

    if (inet_pton(AF_INET6, BR_COAP_ADDR, &sin6->sin6_addr) != 1) {
        ESP_LOGE(TAG, "Failed to parse BR address: %s", BR_COAP_ADDR);
        return false;
    }
    return true;
}

/* ─── Internal: blocking confirmable POST ─────────────────────────────────── */
typedef struct {
    bool           done;
    coap_pdu_code_t response_code;
    uint8_t        *response_data;
    size_t          response_len;
} coap_response_ctx_t;

static void coap_response_handler(coap_session_t *session,
                                   const coap_pdu_t *sent,
                                   const coap_pdu_t *received,
                                   const coap_mid_t  mid)
{
    coap_response_ctx_t *ctx = (coap_response_ctx_t *)
        coap_session_get_app_data(session);

    if (ctx) {
        ctx->response_code = coap_pdu_get_code(received);
        const uint8_t *data;
        size_t len, offset, total;
        if (coap_get_data_large(received, &len, &data, &offset, &total)) {
            if (len > 0 && ctx->response_data) {
                size_t copy = (len < ctx->response_len) ? len : ctx->response_len;
                memcpy(ctx->response_data, data, copy);
                ctx->response_len = copy;
            }
        }
        ctx->done = true;
    }
}

static esp_err_t coap_send_request(coap_request_t method,
                                   const char *uri_path,
                                   const uint8_t *payload,
                                   size_t payload_len,
                                   uint8_t *resp_buf,
                                   size_t *resp_len)
{
    coap_address_t br_addr;
    if (!resolve_br_address(&br_addr)) {
        return ESP_ERR_NOT_FOUND;
    }

    coap_session_t *session = coap_new_client_session(
        s_ctx, NULL, &br_addr, COAP_PROTO_UDP);
    if (!session) {
        ESP_LOGE(TAG, "Failed to create CoAP session");
        return ESP_FAIL;
    }

    coap_response_ctx_t resp_ctx = {
        .done          = false,
        .response_code = 0,
        .response_data = resp_buf,
        .response_len  = resp_len ? *resp_len : 0,
    };
    coap_session_set_app_data(session, &resp_ctx);

    coap_pdu_t *pdu = coap_new_pdu(COAP_MESSAGE_CON, method, session);
    if (!pdu) {
        coap_session_release(session);
        return ESP_ERR_NO_MEM;
    }

    /* Add URI path option */
    coap_optlist_t *optlist = NULL;
    coap_insert_optlist(&optlist,
        coap_new_optlist(COAP_OPTION_URI_PATH,
                         strlen(uri_path), (const uint8_t *)uri_path));
    coap_add_optlist_pdu(pdu, &optlist);
    coap_delete_optlist(optlist);

    /* Content-Format: application/octet-stream */
    if (payload && payload_len) {
        uint8_t cf_buf[2];
        coap_insert_optlist(&optlist,
            coap_new_optlist(COAP_OPTION_CONTENT_FORMAT,
                             coap_encode_var_safe(cf_buf, sizeof(cf_buf),
                                                  COAP_MEDIATYPE_APPLICATION_OCTET_STREAM),
                             cf_buf));
        coap_add_optlist_pdu(pdu, &optlist);
        coap_delete_optlist(optlist);
        coap_add_data(pdu, payload_len, payload);
    }

    coap_register_response_handler(s_ctx, coap_response_handler);

    coap_send(session, pdu);

    /* Drive the CoAP I/O loop until we get a response or timeout */
    int timeout_ms = COAP_RETRY_DELAY_MS * COAP_MAX_RETRIES;
    while (!resp_ctx.done && timeout_ms > 0) {
        coap_io_process(s_ctx, 200);
        timeout_ms -= 200;
    }

    coap_session_release(session);

    if (!resp_ctx.done) {
        ESP_LOGW(TAG, "CoAP request to %s timed out", uri_path);
        return ESP_ERR_TIMEOUT;
    }

    if (resp_len) *resp_len = resp_ctx.response_len;

    ESP_LOGD(TAG, "CoAP %s → response code %d.%02d",
             uri_path,
             COAP_RESPONSE_CLASS(resp_ctx.response_code),
             resp_ctx.response_code & 0x1F);

    return ESP_OK;
}

/* ─── Public API ──────────────────────────────────────────────────────────── */

esp_err_t app_coap_client_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) return ESP_ERR_NO_MEM;

    coap_startup();
    s_ctx = coap_new_context(NULL);
    if (!s_ctx) {
        ESP_LOGE(TAG, "Failed to create CoAP context");
        return ESP_FAIL;
    }

    coap_set_log_level(LOG_WARNING);
    ESP_LOGI(TAG, "CoAP client initialised, BR at %s:%d",
             BR_COAP_ADDR, BR_COAP_PORT);
    return ESP_OK;
}

esp_err_t app_coap_client_register(const char *label)
{
    if (!s_ctx || !s_mutex) return ESP_ERR_INVALID_STATE;

    /* Build the node EUI-64 from Thread stack */
    otInstance   *ot = esp_openthread_get_instance();
    otExtAddress  ext;
    otLinkGetExtendedAddress(ot, &ext);
    uint64_t eui64 = 0;
    for (int i = 0; i < 8; i++) {
        eui64 = (eui64 << 8) | ext.m8[i];
    }

    app_node_reg_payload_t reg = {
        .version           = APP_PROTO_VERSION,
        .sensor_type       = SENSOR_TYPE_TEMP_HUMIDITY,
        .fw_major          = APP_FW_MAJOR,
        .fw_minor          = APP_FW_MINOR,
        .eui64             = eui64,
        .report_interval_s = SENSOR_REPORT_INTERVAL_S,
        .sleep_interval_s  = SENSOR_SLEEP_DURATION_S,
    };
    strncpy(reg.label, label ? label : "node", sizeof(reg.label) - 1);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    esp_err_t ret = coap_send_request(COAP_REQUEST_POST,
                                      COAP_URI_SENSOR_REG,
                                      (uint8_t *)&reg, sizeof(reg),
                                      NULL, NULL);
    xSemaphoreGive(s_mutex);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Node registered: label='%s' eui64=%016llX",
                 reg.label, (unsigned long long)eui64);
    }
    return ret;
}

esp_err_t app_coap_client_send(const app_sensor_reading_t *reading)
{
    if (!s_ctx || !s_mutex) return ESP_ERR_INVALID_STATE;

    otInstance  *ot = esp_openthread_get_instance();
    otExtAddress ext;
    otLinkGetExtendedAddress(ot, &ext);
    uint64_t eui64 = 0;
    for (int i = 0; i < 8; i++) eui64 = (eui64 << 8) | ext.m8[i];

    app_sensor_payload_t payload = {
        .version       = APP_PROTO_VERSION,
        .sensor_type   = SENSOR_TYPE_TEMP_HUMIDITY,
        .node_flags    = 0,
        .eui64         = eui64,
        .uptime_s      = (uint32_t)(esp_timer_get_time() / 1000000ULL),
        .temperature_c = TEMP_FLOAT_TO_RAW(reading->temperature_c),
        .humidity_pct  = HUM_FLOAT_TO_RAW(reading->humidity_pct),
        .battery_mv    = app_sensor_battery_mv(),
        .rssi_dbm      = (int16_t)otLinkGetRssi(ot),
        .seq_num       = s_seq_num++,
    };

    if (payload.battery_mv > 0 && payload.battery_mv < BATTERY_MV_LOW_THRESHOLD) {
        payload.node_flags |= NODE_FLAG_BATTERY_LOW;
    }
    if (!reading->valid) {
        payload.node_flags |= NODE_FLAG_SENSOR_ERROR;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    esp_err_t ret = coap_send_request(COAP_REQUEST_POST,
                                      COAP_URI_SENSOR_DATA,
                                      (uint8_t *)&payload, sizeof(payload),
                                      NULL, NULL);
    xSemaphoreGive(s_mutex);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Telemetry sent: T=%.1f°C RH=%.1f%% seq=%u",
                 reading->temperature_c, reading->humidity_pct, payload.seq_num);
    }
    return ret;
}

esp_err_t app_coap_client_poll_cmd(app_cmd_payload_t *cmd)
{
    if (!s_ctx || !s_mutex || !cmd) return ESP_ERR_INVALID_ARG;

    memset(cmd, 0, sizeof(*cmd));
    cmd->cmd_type = CMD_NONE;

    uint8_t buf[sizeof(app_cmd_payload_t)];
    size_t  len = sizeof(buf);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    esp_err_t ret = coap_send_request(COAP_REQUEST_GET,
                                      COAP_URI_CMD_GET,
                                      NULL, 0,
                                      buf, &len);
    xSemaphoreGive(s_mutex);

    if (ret == ESP_OK && len >= sizeof(app_cmd_payload_t)) {
        memcpy(cmd, buf, sizeof(app_cmd_payload_t));
        if (cmd->cmd_type != CMD_NONE) {
            ESP_LOGI(TAG, "Command received: type=%d id=%d param=%d",
                     cmd->cmd_type, cmd->cmd_id, cmd->cmd_param);
        }
    }
    return ret;
}

void app_coap_client_deinit(void)
{
    if (s_ctx) {
        coap_free_context(s_ctx);
        coap_cleanup();
        s_ctx = NULL;
    }
    if (s_mutex) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }
}
