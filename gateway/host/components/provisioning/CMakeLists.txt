idf_component_register(SRCS "provisioning.c"
                    INCLUDE_DIRS "include"
                    REQUIRES wifi_provisioning esp_wifi nvs_flash esp_event esp_http_client)
