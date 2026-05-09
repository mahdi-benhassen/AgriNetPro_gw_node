cmake_minimum_required(VERSION 3.16)

# Add the ESP Thread Border Router components
list(APPEND EXTRA_COMPONENT_DIRS "$ENV{ESP_THREAD_BR_PATH}/components")

# Add the standard ESP-IDF example components (needed for protocol_examples_common)
list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/examples/common_components/protocol_examples_common")

# Include the main ESP-IDF build system
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(border_router_app)