#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "mqtt_client.h"

// Optional: Global MQTT client handler
esp_mqtt_client_handle_t mqtt_client;

// --- Your Custom AgriNet Gateway Task ---
void agrinet_gateway_task(void *pvParameters) {
    // 1. Setup a UDP socket to listen to the Thread network
    int sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in6 listen_addr = {0};
    listen_addr.sin6_family = AF_INET6;
    listen_addr.sin6_port = htons(1234); // Must match the Node's port
    listen_addr.sin6_addr = in6addr_any; 
    
    bind(sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr));

    char rx_buffer[128];

    while (1) {
        // 2. Wait for incoming sensor data from the ESP32-H2 nodes
        struct sockaddr_in6 source_addr;
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
        
        if (len > 0) {
            rx_buffer[len] = 0; // Null-terminate
            printf("Gateway Received: %s\n", rx_buffer);
            
            // 3. Process the data or forward it to the Cloud via MQTT
            // esp_mqtt_client_publish(mqtt_client, "agrinet/greenhouse1/sensors", rx_buffer, len, 1, 0);
        }
    }
}

void app_main(void)
{
    // ... [Existing Border Router Init Code remains here] ...

    // Optional: Initialize your MQTT Client to the Cloud here
    // mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    // esp_mqtt_client_start(mqtt_client);

    // Spawn your Gateway logic task
    xTaskCreate(agrinet_gateway_task, "agrinet_gateway", 4096, NULL, 5, NULL);
}