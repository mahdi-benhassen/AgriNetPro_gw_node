#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h" // You can use standard UDP sockets!

// --- Your Custom AgriNet Task ---
void agrinet_sensor_task(void *pvParameters) {
    while (1) {
        // 1. Read your sensor (e.g., I2C, SPI, ADC)
        float temp = 24.5; // Replace with actual sensor read
        float humidity = 60.2;
        
        // 2. Format the data
        char payload[100];
        sprintf(payload, "{\"temp\": %.2f, \"hum\": %.2f}", temp, humidity);
        
        // 3. Send over Thread via UDP (Standard IPv6 Socket)
        int sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
        struct sockaddr_in6 dest_addr = {0};
        dest_addr.sin6_family = AF_INET6;
        dest_addr.sin6_port = htons(1234); // Your Gateway's listening port
        inet6_aton("ff03::1", &dest_addr.sin6_addr); // Example Multicast, or use Gateway's specific IPv6
        
        sendto(sock, payload, strlen(payload), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        close(sock);

        printf("AgriNet Node: Sent data -> %s\n", payload);

        // 4. Sleep for 5 minutes before reading again
        vTaskDelay(pdMS_TO_TICKS(300000)); 
    }
}

void app_main(void)
{
    // ... [Existing OpenThread Init Code remains here] ...

    // Spawn your application on top of the SDK!
    xTaskCreate(agrinet_sensor_task, "agrinet_sensor", 4096, NULL, 5, NULL);
}