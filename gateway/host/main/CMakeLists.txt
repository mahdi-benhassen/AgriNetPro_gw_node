idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES hal_actuator network_manager rule_engine
                             zigbee_coordinator provisioning ota_manager nvs_flash)
