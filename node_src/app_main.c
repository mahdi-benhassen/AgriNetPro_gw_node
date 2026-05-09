#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "esp_thread.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "../common_src/config.h"
#include "../common_src/sensor_types.h"
#include "../common_src/messaging.h"
#include "sensors/sensors.h"

static const char *TAG = "SENSOR_NODE";

// Device information
static DeviceInfo_t device_info = {
    .device_type = THREAD_DEVICE_TYPE_NODE,
    .firmware_version = 1,
    .num_sensors = 2,
    .sensors = {SENSOR_TYPE_TEMPERATURE, SENSOR_TYPE_HUMIDITY}
};

// Sensor drivers
static TempSensorDriver_t *temp_driver;
static HumiditySensorDriver_t *hum_driver;

// Message queue for outgoing messages
static MessageQueue_t msg_queue;
static QueueHandle_t thread_tx_queue = NULL;

// Network status
static NetworkStats_t net_stats = {
    .status = NETWORK_STATUS_DISCONNECTED,
    .link_quality = 0,
    .uptime_ms = 0,
    .message_count = 0
};

// ============= Thread Network Functions =============

static void thread_state_changed_callback(otChangedFlags changed_flags, void *context)
{
    otInstance *instance = (otInstance *)context;
    
    if (changed_flags & OT_CHANGED_THREAD_NETDATA) {
        if (otThreadGetDeviceRole(instance) != OT_DEVICE_ROLE_DISABLED) {
            ESP_LOGI(TAG, "Thread device joined network!");
            net_stats.status = NETWORK_STATUS_CONNECTED;
        }
    }
    
    if (changed_flags & OT_CHANGED_THREAD_ROLE) {
        otDeviceRole role = otThreadGetDeviceRole(instance);
        ESP_LOGI(TAG, "Device role changed: %d", role);
    }
}

static int thread_send_message(Message_t *msg)
{
    // Send via Thread CoAP
    // This is a placeholder - actual implementation depends on CoAP setup
    ESP_LOGI(TAG, "Sending Thread message, type=%d, length=%d", msg->header.type, msg->header.length);
    net_stats.message_count++;
    return 0;
}

// ============= Sensor Reading Task =============

static void sensor_reading_task(void *arg)
{
    float temperature = 0.0f;
    float humidity = 0.0f;
    int result = 0;
    
    ESP_LOGI(TAG, "Sensor reading task started");
    
    // Initialize sensors
    if (temp_driver->init() != 0) {
        ESP_LOGE(TAG, "Failed to initialize temperature sensor");
    }
    
    if (hum_driver->init() != 0) {
        ESP_LOGE(TAG, "Failed to initialize humidity sensor");
    }
    
    while (1) {
        // Read temperature
        result = temp_driver->read(&temperature);
        if (result == 0) {
            ESP_LOGI(TAG, "Temperature: %.2f°C", temperature);
            
            // Create sensor message
            Message_t msg;
            uint16_t msg_len = messaging_create_sensor_message(
                &msg, 
                device_info.device_id,
                SENSOR_TYPE_TEMPERATURE,
                temperature,
                0xFFFFFFFFFFFFFFFFULL  // Broadcast to all gateways
            );
            
            if (msg_len > 0 && thread_tx_queue != NULL) {
                xQueueSend(thread_tx_queue, &msg, pdMS_TO_TICKS(100));
            }
        } else {
            ESP_LOGE(TAG, "Failed to read temperature");
        }
        
        // Read humidity
        result = hum_driver->read(&humidity);
        if (result == 0) {
            ESP_LOGI(TAG, "Humidity: %.2f%%", humidity);
            
            // Create sensor message
            Message_t msg;
            uint16_t msg_len = messaging_create_sensor_message(
                &msg,
                device_info.device_id,
                SENSOR_TYPE_HUMIDITY,
                humidity,
                0xFFFFFFFFFFFFFFFFULL  // Broadcast to all gateways
            );
            
            if (msg_len > 0 && thread_tx_queue != NULL) {
                xQueueSend(thread_tx_queue, &msg, pdMS_TO_TICKS(100));
            }
        } else {
            ESP_LOGE(TAG, "Failed to read humidity");
        }
        
        vTaskDelay(pdMS_TO_TICKS(TEMP_SENSOR_POLL_INTERVAL_MS));
    }
}

// ============= Thread Message TX Task =============

static void thread_tx_task(void *arg)
{
    Message_t msg;
    otInstance *instance = (otInstance *)arg;
    
    ESP_LOGI(TAG, "Thread TX task started");
    
    while (1) {
        if (xQueueReceive(thread_tx_queue, &msg, pdMS_TO_TICKS(1000))) {
            if (net_stats.status == NETWORK_STATUS_CONNECTED) {
                thread_send_message(&msg);
            } else {
                ESP_LOGW(TAG, "Network not connected, message dropped");
            }
        }
    }
}

// ============= Initialization =============

void app_main(void)
{
    ESP_LOGI(TAG, "=== AgriNetPro Sensor Node Application ===");
    ESP_LOGI(TAG, "Device Type: Sensor Node (Temperature & Humidity)");
    
    // Get sensor drivers
    temp_driver = get_temperature_driver();
    hum_driver = get_humidity_driver();
    
    ESP_LOGI(TAG, "Temperature Sensor: %s", temp_driver->name);
    ESP_LOGI(TAG, "Humidity Sensor: %s", hum_driver->name);
    
    // Initialize message queue
    message_queue_init(&msg_queue);
    
    // Create message TX queue for Thread
    thread_tx_queue = xQueueCreate(10, sizeof(Message_t));
    if (thread_tx_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create TX queue");
        return;
    }
    
    // Initialize Thread device
    otInstance *instance = otInstanceInitSingle();
    if (instance == NULL) {
        ESP_LOGE(TAG, "Failed to initialize Thread instance");
        return;
    }
    
    // Set device as sleepy end device (FED)
    otThreadSetLinkMode(instance, (otLinkModeConfig){
        .mRxOnWhenIdle = false,
        .mSecureDataRequests = true,
        .mDeviceType = false,
        .mNetworkData = false,
    });
    
    // Register state change callback
    otSetStateChangedCallback(instance, thread_state_changed_callback, instance);
    
    // Start Thread
    otIp6SetEnabled(instance, true);
    otThreadSetEnabled(instance, true);
    
    ESP_LOGI(TAG, "Thread enabled, waiting for network join...");
    
    // Create sensor reading task
    xTaskCreate(sensor_reading_task, "sensor_read", 4096, NULL, 5, NULL);
    
    // Create Thread TX task
    xTaskCreate(thread_tx_task, "thread_tx", 4096, instance, 4, NULL);
    
    ESP_LOGI(TAG, "Node initialization complete");
}
