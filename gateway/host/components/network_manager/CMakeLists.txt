idf_component_register(SRCS "network_manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_wifi mqtt spiffs esp_netif esp_event nvs_flash)
