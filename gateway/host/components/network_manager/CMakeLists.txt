idf_component_register(SRCS "network_manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_wifi espressif__esp-mqtt spiffs esp_netif esp_event)
