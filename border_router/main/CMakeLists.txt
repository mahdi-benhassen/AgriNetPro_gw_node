idf_component_register(
    SRCS
        "app_br_main.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        nvs_flash
        esp_event
        esp_netif
        esp_ot_br            # from esp-thread-br SDK
        openthread           # This provides all esp_openthread.h APIs
        protocol_examples_common
        app_device_registry
        app_coap_server
        app_mqtt_bridge
        app_rest_api
        esp_timer
        freertos
)