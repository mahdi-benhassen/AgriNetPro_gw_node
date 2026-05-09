/**
 * @file app_node_main.c
 * @brief Main application for the Thread sensor node (ESP32-H2).
 *
 * Startup sequence:
 *   1. NVS / nvs_flash_init
 *   2. Check deep-sleep wakeup vs. cold boot
 *   3. Initialize OpenThread stack (via esp_openthread_init)
 *   4. Attach to Thread network (using dataset from NVS or menuconfig)
 *   5. Initialize sensor hardware
 *   6. Initialize CoAP client
 *   7. Register node with border router (cold boot only)
 *   8. Main loop: read → send → poll commands → sleep
 *
 * This file wires the esp-thread-br SDK into the application layer.
 */

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

/* OpenThread / ESP-IDF Thread integration */
#include "esp_openthread.h"
#include "esp_vfs_eventfd.h"
#include "esp_ot_config.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_openthread_types.h"
#include "openthread/instance.h"
#include "openthread/logging.h"
#include "openthread/tasklet.h"
#include "openthread/thread.h"
#include "openthread/dataset_ftd.h"
#include "openthread/dataset.h"

/* Application layers */
#include "app_config.h"
#include "app_protocol.h"
#include "app_sensor.h"
#include "app_coap_client.h"
#include "app_sleep.h"

static const char *TAG = TAG_MAIN;

/* ─── Kconfig-settable node label ─────────────────────────────────────────── */
#ifndef CONFIG_APP_NODE_LABEL
#define CONFIG_APP_NODE_LABEL "SensorNode-01"
#endif

/* ─── OpenThread task  ─────────────────────────────────────────────────────── */
static void ot_task(void *arg)
{
    esp_openthread_platform_config_t ot_cfg = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config  = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config  = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };

    /* Launch OpenThread main loop — this never returns */
    ESP_ERROR_CHECK(esp_openthread_init(&ot_cfg));
    ESP_ERROR_CHECK(esp_openthread_auto_start(NULL));   /* Joins / forms network */
    esp_openthread_launch_mainloop();
    esp_openthread_deinit();
    vTaskDelete(NULL);
}

/* ─── Wait until Thread is attached ─────────────────────────────────────────  */
static void wait_for_thread_attach(void)
{
    otInstance *ot;
    ESP_LOGI(TAG, "Waiting for Thread network attach…");
    while (1) {
        esp_openthread_lock_acquire(portMAX_DELAY);
        ot = esp_openthread_get_instance();
        otDeviceRole role = otThreadGetDeviceRole(ot);
        esp_openthread_lock_release();

        if (role >= OT_DEVICE_ROLE_CHILD) {
            ESP_LOGI(TAG, "Thread attached! Role: %s",
                     otThreadDeviceRoleToString(role));
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ─── Execute a downlink command ──────────────────────────────────────────── */
static void handle_command(const app_cmd_payload_t *cmd)
{
    switch (cmd->cmd_type) {
    case CMD_REBOOT:
        ESP_LOGI(TAG, "CMD: Reboot requested");
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
        break;
    case CMD_LED_ON:
        ESP_LOGI(TAG, "CMD: LED ON");
        gpio_set_level(APP_LED_GPIO, 1);
        break;
    case CMD_LED_OFF:
        ESP_LOGI(TAG, "CMD: LED OFF");
        gpio_set_level(APP_LED_GPIO, 0);
        break;
    case CMD_SET_INTERVAL:
        ESP_LOGI(TAG, "CMD: Set interval → %d s", cmd->cmd_param);
        /* In production: update NVS and apply to sleep timer */
        break;
    case CMD_OTA_START:
        ESP_LOGI(TAG, "CMD: OTA start from URL: %s", cmd->cmd_payload);
        /* Trigger esp_https_ota() here */
        break;
    default:
        break;
    }
}

/* ─── Sensor + CoAP task ─────────────────────────────────────────────────── */
static void sensor_task(void *arg)
{
    bool is_wakeup = app_sleep_is_wakeup_from_deep();

    /* --- Wait for Thread to be ready ----------------------------------- */
    wait_for_thread_attach();

    /* --- First-boot registration --------------------------------------- */
    if (!is_wakeup) {
        ESP_LOGI(TAG, "Cold boot — registering with border router");
        esp_err_t ret = app_coap_client_register(CONFIG_APP_NODE_LABEL);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Registration failed (will retry next cycle): %s",
                     esp_err_to_name(ret));
        }
    }

    /* --- Main measurement loop ----------------------------------------- */
    while (1) {
        /* 1. Read sensor */
        app_sensor_reading_t reading;
        esp_err_t sret = app_sensor_read(&reading);
        if (sret != ESP_OK) {
            ESP_LOGW(TAG, "Sensor read error — using last good value");
            reading = app_sensor_last_reading();
            reading.valid = false;
        }

        /* 2. Upload over CoAP */
        for (int attempt = 0; attempt < COAP_MAX_RETRIES; attempt++) {
            esp_err_t cret = app_coap_client_send(&reading);
            if (cret == ESP_OK) break;
            ESP_LOGW(TAG, "CoAP send attempt %d/%d failed: %s",
                     attempt + 1, COAP_MAX_RETRIES, esp_err_to_name(cret));
            vTaskDelay(pdMS_TO_TICKS(COAP_RETRY_DELAY_MS));
        }

        /* 3. Poll for downlink command */
        app_cmd_payload_t cmd;
        if (app_coap_client_poll_cmd(&cmd) == ESP_OK &&
            cmd.cmd_type != CMD_NONE) {
            handle_command(&cmd);
        }

        /* 4. Sleep until next report */
        app_sleep_enter(SENSOR_SLEEP_DURATION_S * 1000UL);
    }
}

/* ─── app_main ───────────────────────────────────────────────────────────── */
void app_main(void)
{
    ESP_LOGI(TAG, "ESP Thread Sensor Node v%d.%d.%d starting…",
             APP_FW_MAJOR, APP_FW_MINOR, APP_FW_PATCH);

    /* NVS — required by WiFi, Thread, and OTA */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* LED GPIO */
    gpio_reset_pin(APP_LED_GPIO);
    gpio_set_direction(APP_LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(APP_LED_GPIO, 0);

    /* Sensor hardware */
    ESP_ERROR_CHECK(app_sensor_init());

    /* Sleep subsystem — configure Thread as Sleepy End Device */
    ESP_ERROR_CHECK(app_sleep_init(SLEEP_MODE_LIGHT));

    /* CoAP client */
    ESP_ERROR_CHECK(app_coap_client_init());

    /* --- OpenThread Initialization (ESP-IDF v5.5) --- */
    ESP_ERROR_CHECK(esp_netif_init());
    
    esp_vfs_eventfd_config_t eventfd_config = ESP_VFS_EVENTD_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));

    /* Launch OpenThread task on core 0 */
    xTaskCreate(ot_task, "ot_task", 10240, NULL, 5, NULL);

    /* Sensor + upload task on core 1 (higher priority than openthread) */
    xTaskCreate(sensor_task, "sensor_task", 8192, NULL, 4, NULL);

    ESP_LOGI(TAG, "Tasks launched — node is running.");
}
