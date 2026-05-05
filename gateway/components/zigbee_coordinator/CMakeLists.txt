idf_component_register(SRCS "zigbee_coordinator.c"
                    INCLUDE_DIRS "include"
                    REQUIRES espressif__esp-zigbee-lib mbedtls)

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-error=cpp")

# Manually add compat include path since it might not be propagating correctly
target_include_directories(${COMPONENT_LIB} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../managed_components/espressif__esp-zigbee-lib/include/compat")
