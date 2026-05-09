# Add the ESP Thread Border Router components
list(APPEND EXTRA_COMPONENT_DIRS "$ENV{ESP_THREAD_BR_PATH}/components")

# ADD THIS LINE: Add the standard ESP-IDF example components
list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/examples/common_components/protocol_examples_common")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(border_router_app) # (Your project name might be slightly different here, keep yours)

idf_component_register(
    SRCS
        "app_br_main.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        nvs_flash
        esp_event
        esp_netif
        openthread           # This provides all OpenThread & BR APIs
        protocol_examples_common
        app_device_registry
        app_coap_server
        app_mqtt_bridge
        app_rest_api
        esp_timer
        freertos
)