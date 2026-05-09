idf_component_register(
    SRCS  "app_coap_server.c"
    INCLUDE_DIRS "include"
    REQUIRES coap freertos log app_device_registry
)
