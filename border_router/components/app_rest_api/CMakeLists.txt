idf_component_register(
    SRCS  "app_rest_api.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server cJSON log esp_timer app_device_registry
)
