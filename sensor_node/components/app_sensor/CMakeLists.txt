set(srcs "app_sensor.c")

if(CONFIG_APP_SENSOR_TYPE_DHT22)
    list(APPEND srcs "driver_dht22.c")
elseif(CONFIG_APP_SENSOR_TYPE_SHT31)
    list(APPEND srcs "driver_sht31.c")
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "include"
    REQUIRES driver esp_adc log freertos
)
