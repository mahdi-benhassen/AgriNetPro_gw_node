idf_component_register(
    SRCS  "app_mqtt_bridge.c"
    INCLUDE_DIRS "include"
    REQUIRES mqtt esp_event cJSON log app_device_registry freertos
)
