idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES hal_actuator network_manager rule_engine
                             zigbee_coordinator nvs_flash mbedtls)

# Add shim include paths to zigbee library for ESP-IDF 6.0 compatibility
idf_component_get_property(zigbee_lib espressif__esp-zigbee-lib COMPONENT_LIB)
target_include_directories(${zigbee_lib} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../include_shims")
target_include_directories(${zigbee_lib} PUBLIC "C:/esp/v6.0/esp-idf/components/mbedtls/port/include")
target_include_directories(${zigbee_lib} PUBLIC "C:/esp/v6.0/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/drivers/builtin/include")
