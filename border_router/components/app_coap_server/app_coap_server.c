/**
 * @file app_coap_server.c
 * @brief CoAP server on the ESP Thread Border Router.
 *
 * Listens on UDP port 5683 on the Thread mesh-local interface.
 * Handles:
 *   POST /sensor/register  → app_device_registry_register()
 *   POST /sensor/data      → app_device_registry_update()
 *   GET  /cmd              → app_device_registry_pop_cmd()
 *
 * Runs in its own FreeRTOS task using the libcoap library from ESP-IDF.
 */

#include "app_coap_server.h"
#include "app_config.h"
#include "app_protocol.h"
#include "app_device_registry.h"
#include "esp_log.h"
#include "coap3/coap.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

static const char *TAG = TAG_COAP;

/* ─── Helper: extract IPv6 source address as string ─────────────────────── */
static void session_ipv6_str(coap_session_t *session, char *buf, size_t len)
{
    const coap_address_t *addr = coap_session_get_addr_remote(session);
    if (addr && addr->addr.sa.sa_family == AF_INET6) {
        inet_ntop(AF_INET6,
                  &((struct sockaddr_in6 *)&addr->addr)->sin6_addr,
                  buf, (socklen_t)len);
    } else {
        strncpy(buf, "unknown", len);
    }
}

/* ─── POST /sensor/register ─────────────────────────────────────────────── */
static void handler_sensor_register(coap_resource_t *resource,
                                     coap_session_t  *session,
                                     const coap_pdu_t *request,
                                     const coap_string_t *query,
                                     coap_pdu_t *response)
{
    const uint8_t *data;
    size_t len, offset, total;

    if (!coap_get_data_large(request, &len, &data, &offset, &total) ||
        len < sizeof(app_node_reg_payload_t)) {
        ESP_LOGW(TAG, "/sensor/register: bad payload length %zu", len);
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_BAD_REQUEST);
        return;
    }

    char ipv6[DEVICE_ADDR_LEN];
    session_ipv6_str(session, ipv6, sizeof(ipv6));

    const app_node_reg_payload_t *reg = (const app_node_reg_payload_t *)data;
    esp_err_t ret = app_device_registry_register(reg, ipv6);

    coap_pdu_set_code(response,
                      ret == ESP_OK ? COAP_RESPONSE_CODE_CREATED
                                    : COAP_RESPONSE_CODE_INTERNAL_ERROR);
}

/* ─── POST /sensor/data ─────────────────────────────────────────────────── */
static void handler_sensor_data(coap_resource_t *resource,
                                 coap_session_t  *session,
                                 const coap_pdu_t *request,
                                 const coap_string_t *query,
                                 coap_pdu_t *response)
{
    const uint8_t *data;
    size_t len, offset, total;

    if (!coap_get_data_large(request, &len, &data, &offset, &total) ||
        len < sizeof(app_sensor_payload_t)) {
        ESP_LOGW(TAG, "/sensor/data: bad payload length %zu", len);
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_BAD_REQUEST);
        return;
    }

    if (((app_sensor_payload_t *)data)->version != APP_PROTO_VERSION) {
        ESP_LOGW(TAG, "/sensor/data: version mismatch (got %d, expected %d)",
                 ((app_sensor_payload_t *)data)->version, APP_PROTO_VERSION);
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_BAD_REQUEST);
        return;
    }

    char ipv6[DEVICE_ADDR_LEN];
    session_ipv6_str(session, ipv6, sizeof(ipv6));

    const app_sensor_payload_t *payload = (const app_sensor_payload_t *)data;
    esp_err_t ret = app_device_registry_update(payload, ipv6);

    coap_pdu_set_code(response,
                      ret == ESP_OK ? COAP_RESPONSE_CODE_CHANGED
                                    : COAP_RESPONSE_CODE_INTERNAL_ERROR);
}

/* ─── GET /cmd ──────────────────────────────────────────────────────────── */
static void handler_cmd_get(coap_resource_t *resource,
                             coap_session_t  *session,
                             const coap_pdu_t *request,
                             const coap_string_t *query,
                             coap_pdu_t *response)
{
    /*
     * The EUI-64 of the requesting node is embedded in the request query:
     * /cmd?eui=<hex16>
     * In a full implementation, parse query. Here we read from the CoAP
     * payload which carries the EUI-64 (4 bytes for simplicity demo).
     */
    const uint8_t *data;
    size_t len, offset, total;
    uint64_t eui64 = 0;

    if (coap_get_data_large(request, &len, &data, &offset, &total) &&
        len >= sizeof(uint64_t)) {
        memcpy(&eui64, data, sizeof(eui64));
    } else {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_BAD_REQUEST);
        return;
    }

    app_cmd_payload_t cmd;
    app_device_registry_pop_cmd(eui64, &cmd);

    coap_add_data(response, sizeof(cmd), (const uint8_t *)&cmd);
    coap_pdu_set_code(response, COAP_RESPONSE_CODE_CONTENT);
}

/* ─── CoAP server task ──────────────────────────────────────────────────── */
static void coap_server_task(void *arg)
{
    coap_context_t *ctx = NULL;
    coap_endpoint_t *ep = NULL;

    coap_startup();
    coap_set_log_level(LOG_WARNING);

    ctx = coap_new_context(NULL);
    if (!ctx) {
        ESP_LOGE(TAG, "Failed to create CoAP context");
        vTaskDelete(NULL);
        return;
    }

    /* Bind to all interfaces on port 5683 */
    coap_address_t addr;
    memset(&addr, 0, sizeof(addr));
    addr.size = sizeof(struct sockaddr_in6);
    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&addr.addr;
    sin6->sin6_family = AF_INET6;
    sin6->sin6_addr   = in6addr_any;
    sin6->sin6_port   = htons(BR_COAP_PORT);

    ep = coap_new_endpoint(ctx, &addr, COAP_PROTO_UDP);
    if (!ep) {
        ESP_LOGE(TAG, "Failed to bind CoAP endpoint on port %d", BR_COAP_PORT);
        coap_free_context(ctx);
        vTaskDelete(NULL);
        return;
    }

    /* Register resources */
    coap_resource_t *r;

    r = coap_resource_init(coap_make_str_const(COAP_URI_SENSOR_REG + 1), 0);
    coap_register_handler(r, COAP_REQUEST_POST, handler_sensor_register);
    coap_add_resource(ctx, r);

    r = coap_resource_init(coap_make_str_const(COAP_URI_SENSOR_DATA + 1), 0);
    coap_register_handler(r, COAP_REQUEST_POST, handler_sensor_data);
    coap_add_resource(ctx, r);

    r = coap_resource_init(coap_make_str_const(COAP_URI_CMD_GET + 1), 0);
    coap_register_handler(r, COAP_REQUEST_GET, handler_cmd_get);
    coap_add_resource(ctx, r);

    ESP_LOGI(TAG, "CoAP server listening on [::]:%d", BR_COAP_PORT);

    while (1) {
        coap_io_process(ctx, 1000);   /* 1-second I/O timeout */
    }

    coap_free_context(ctx);
    coap_cleanup();
    vTaskDelete(NULL);
}

/* ─── Public API ─────────────────────────────────────────────────────────── */

esp_err_t app_coap_server_start(void)
{
    BaseType_t r = xTaskCreate(coap_server_task, "coap_server",
                               8192, NULL, 5, NULL);
    return (r == pdPASS) ? ESP_OK : ESP_FAIL;
}
