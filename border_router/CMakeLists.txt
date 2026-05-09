cmake_minimum_required(VERSION 3.16)

# Add the ESP Thread Border Router components
list(APPEND EXTRA_COMPONENT_DIRS "$ENV{ESP_THREAD_BR_PATH}/components")

# Add the standard ESP-IDF example components
list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/examples/common_components/protocol_examples_common")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(border_router_app)