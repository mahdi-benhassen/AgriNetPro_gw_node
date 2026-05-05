cmake_minimum_required(VERSION 3.16)

# Include ESP-IDF project configuration
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Set the project name
project(agrinetpro_gateway)

# Add shim include paths for compatibility with ESP-IDF 6.0 mbedtls
add_compile_options("-I${CMAKE_CURRENT_SOURCE_DIR}/include_shims")
add_compile_options("-IC:/esp/v6.0/esp-idf/components/mbedtls/mbedtls/tf-psa-crypto/drivers/builtin/include")
