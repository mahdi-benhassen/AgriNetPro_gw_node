idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES hal_sensor power_manager zigbee_node nvs_flash)
