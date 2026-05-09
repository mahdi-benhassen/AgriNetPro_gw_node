cmake_minimum_required(VERSION 3.16)

# Bring in ESP-IDF and the esp-thread-br SDK
# Set IDF_PATH before calling this, e.g.:
#   export IDF_PATH=~/esp/esp-idf
#   export ESP_THREAD_BR_PATH=~/esp/esp-thread-br

set(EXTRA_COMPONENT_DIRS
    "$ENV{ESP_THREAD_BR_PATH}/components"   # esp-thread-br SDK components
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/../common"
    "$ENV{IDF_PATH}/examples/common_components/protocol_examples_common"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(sensor_node)

# Ensure the common header directory is on the include path for all components
idf_build_set_property(COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/../common" APPEND)
