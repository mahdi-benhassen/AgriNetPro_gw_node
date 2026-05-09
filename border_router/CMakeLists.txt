cmake_minimum_required(VERSION 3.16)

# ─── Build the ESP Thread Border Router with the application layer ────────────
#
# This project wraps the basic_thread_border_router example from esp-thread-br
# and adds our application components on top.
#
# Required environment variables:
#   IDF_PATH          → path to esp-idf
#   ESP_THREAD_BR_PATH → path to esp-thread-br (cloned from GitHub)
#
# Build:
#   cd border_router
#   idf.py set-target esp32s3
#   idf.py build
#   idf.py -p /dev/ttyUSB0 flash monitor

set(EXTRA_COMPONENT_DIRS
    # esp-thread-br SDK components (Border Router library, RCP update, etc.)
    "$ENV{ESP_THREAD_BR_PATH}/components"

    # Our application-layer components
    "${CMAKE_CURRENT_SOURCE_DIR}/components"

    # Shared protocol definitions
    "${CMAKE_CURRENT_SOURCE_DIR}/../common"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(border_router_app)

# Expose the common headers to every component in this build
idf_build_set_property(
    COMPILE_OPTIONS
    "-I${CMAKE_CURRENT_SOURCE_DIR}/../common"
    APPEND
)
