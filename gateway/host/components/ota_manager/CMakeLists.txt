idf_component_register(SRCS "ota_manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_https_ota esp_ota_ops nvs_flash)
