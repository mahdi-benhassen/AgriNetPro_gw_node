idf_component_register(
    SRCS
        "app_br_main.c"
    INCLUDE_DIRS
        "."
        "../../common"
    REQUIRES
        nvs_flash
        esp_event
        esp_netif
        openthread           
        protocol_examples_common
        app_device_registry
        app_coap_server
        app_mqtt_bridge
        app_rest_api
        esp_timer
        freertos
        log
        cjson
        vfs
        esp_driver_uart
        mdns
)

# Force openthread to depend on mdns and lwip so the linker places them AFTER openthread.
# This resolves "undefined reference to mdns_query_async_new" and "netif_set_status_callback".
idf_component_get_property(ot_lib openthread COMPONENT_LIB)
idf_component_get_property(mdns_lib mdns COMPONENT_LIB)
idf_component_get_property(lwip_lib lwip COMPONENT_LIB)

if(ot_lib)
    if(mdns_lib)
        target_link_libraries(${ot_lib} PUBLIC ${mdns_lib})
    endif()
    if(lwip_lib)
        target_link_libraries(${ot_lib} PUBLIC ${lwip_lib})
    endif()
endif()