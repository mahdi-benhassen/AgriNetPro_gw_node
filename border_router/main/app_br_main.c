/**
 * @file app_br_main.c
 * @brief Border Router application entry point.
 *
 * Layers:
 *   ┌──────────────────────────────────────────────┐
 *   │  Application Layer  (this repo)              │
 *   │  ┌────────────┐ ┌──────────┐ ┌───────────┐  │
 *   │  │ CoAP Server│ │MQTT Bridge│ │ REST API  │  │
 *   │  └────────────┘ └──────────┘ └───────────┘  │
 *   │  ┌────────────────────────────────────────┐  │
 *   │  │        Device Registry                  │  │
 *   │  └────────────────────────────────────────┘  │
 *   ├──────────────────────────────────────────────┤
 *   │  ESP Thread Border Router SDK (esp-thread-br)│
 *   │  basic_thread_border_router example          │
 *   ├──────────────────────────────────────────────┤
 *   │  ESP-IDF  +  OpenThread  +  Wi-Fi            │
 *   └──────────────────────────────────────────────┘
 *
 * Key decision: we call esp_openthread_border_router_init() AFTER Wi-Fi has
 * obtained an IP address, exactly as the basic_thread_border_router example
 * does.  Our application services (CoAP, MQTT, REST) start after the BR is
 * fully operational.
 */

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

/* Wi-Fi / Ethernet connection helper (from ESP-IDF examples) */
#include "protocol_examples_common.h"

/* ESP Thread Border Router SDK */
#include "esp_openthread.h"
#include "esp_openthread_border_router.h"
#include "esp_openthread_defaults.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_openthread_types.h"
#include "openthread/border_router.h"
#include "openthread/instance.h"
#include "openthread/logging.h"
#include "openthread/tasklet.h"
#include "openthread/thread.h"
#include "openthread/dataset_ftd.h"

/* Application layers */
#include "app_config.h"
#include "app_device_registry.h"
#include "app_coap_server.h"
#include "app_mqtt_bridge.h"
#include "app_rest_api.h"

static const char *TAG = TAG_MAIN;

/* ─── OpenThread + Border Router task ────────────────────────────────────── */

static esp_netif_t *s_thread_netif = NULL;

static void ot_br_task(void *arg)
{
    esp_openthread_platform_config_t ot_cfg = {
        /* Use the defaults provided by esp-thread-br SDK.
         * These configure UART to ESP32-H2 RCP automatically. */
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config  = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config  = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };

    /* Init OpenThread core */
    ESP_ERROR_CHECK(esp_openthread_init(&ot_cfg));

    /* Attach Thread netif */
    s_thread_netif = esp_netif_create_default_openthread();
    ESP_ERROR_CHECK(esp_openthread_netif_glue_init(s_thread_netif));

    /* Init Border Router (bi-directional routing, NAT64, SRP, mDNS) */
    ESP_ERROR_CHECK(esp_openthread_border_router_init());

    /* Auto-start: form or join Thread network using NVS dataset */
    ESP_ERROR_CHECK(esp_openthread_auto_start(NULL));

    ESP_LOGI(TAG, "OpenThread Border Router running");

    /* This never returns — drives the OpenThread task loop */
    esp_openthread_launch_mainloop();

    /* Cleanup (never reached) */
    esp_openthread_netif_glue_deinit(s_thread_netif);
    esp_netif_destroy(s_thread_netif);
    esp_openthread_deinit();
    vTaskDelete(NULL);
}

/* ─── Periodic offline-sweep timer ──────────────────────────────────────── */

static void sweep_timer_cb(TimerHandle_t tmr)
{
    app_device_registry_sweep_offline();
}

/* ─── Wait for IP address on the uplink interface ────────────────────────── */

static EventGroupHandle_t s_ip_event_group;
#define IP_CONNECTED_BIT  BIT0

static void on_ip_event(void *arg, esp_event_base_t base,
                         int32_t id, void *data)
{
    if (base == IP_EVENT && (id == IP_EVENT_STA_GOT_IP ||
                              id == IP_EVENT_ETH_GOT_IP)) {
        xEventGroupSetBits(s_ip_event_group, IP_CONNECTED_BIT);
    }
}

/* ─── app_main ───────────────────────────────────────────────────────────── */

void app_main(void)
{
    ESP_LOGI(TAG, "ESP Thread Border Router + Application Layer v%d.%d.%d",
             APP_FW_MAJOR, APP_FW_MINOR, APP_FW_PATCH);

    /* ── 1. System init ──────────────────────────────────────────────────── */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* ── 2. Connect to Wi-Fi (or Ethernet) ──────────────────────────────── */
    s_ip_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,
                                               ESP_EVENT_ANY_ID,
                                               on_ip_event, NULL));

    /* example_connect() from ESP-IDF handles Wi-Fi SSID/PSK from menuconfig */
    ESP_ERROR_CHECK(example_connect());

    /* Wait for IP before starting Thread (BR needs uplink routing) */
    xEventGroupWaitBits(s_ip_event_group, IP_CONNECTED_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "Uplink connected — starting Thread Border Router");

    /* ── 3. Application services init ────────────────────────────────────── */
    ESP_ERROR_CHECK(app_device_registry_init());

    /* ── 4. Launch OpenThread + Border Router ─────────────────────────────── */
    /* Stack size 10 KB, priority 5 (must be > than Wi-Fi task) */
    xTaskCreate(ot_br_task, "ot_br_task", 10240, NULL, 5, NULL);

    /* Give OT a moment to initialise before starting CoAP listener */
    vTaskDelay(pdMS_TO_TICKS(3000));

    /* ── 5. Start application services ──────────────────────────────────── */
    ESP_ERROR_CHECK(app_coap_server_start());
    ESP_ERROR_CHECK(app_mqtt_bridge_start());
    ESP_ERROR_CHECK(app_rest_api_start());

    /* ── 6. Periodic offline sweep (every 30 s) ──────────────────────────── */
    TimerHandle_t sweep_timer = xTimerCreate("sweep",
                                             pdMS_TO_TICKS(30000),
                                             pdTRUE, NULL, sweep_timer_cb);
    xTimerStart(sweep_timer, 0);

    ESP_LOGI(TAG,
             "\n"
             "=========================================================\n"
             "  ESP Thread Application Layer — Ready\n"
             "  CoAP server : [Thread mesh-local]:%d\n"
             "  REST API    : http://<br-ip>:%d/api/v1/nodes\n"
             "  MQTT broker : %s\n"
             "=========================================================",
             BR_COAP_PORT, REST_API_PORT, MQTT_BROKER_URI);
}
