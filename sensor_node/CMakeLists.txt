cmake_minimum_required(VERSION 3.16)

# Sensor Node — pure Thread end-device (ESP32-H2).
# Only the built-in IDF openthread component is needed; do NOT include the
# esp-thread-br Border Router SDK here, as it pulls in esp_ot_br_server and
# its SPIFFS web partition which do not belong on a leaf node.

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/../common"
    "$ENV{IDF_PATH}/examples/common_components/protocol_examples_common"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(sensor_node)

# Ensure the common header directory is on the include path for all components
idf_build_set_property(COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/../common" APPEND)
