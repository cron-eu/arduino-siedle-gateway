cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# PROJECT_VER is taken from `git describe`, no need to set it here
project(siedle-gateway)

# Per-device configuration (AWS IoT identity, setup hotspot password), flashed into its own NVS partition so
# the application image stays identical for all devices and OTA updates never touch it. See devcfg/README.md.
set(DEVCFG_DIR "${CMAKE_CURRENT_LIST_DIR}/devcfg" CACHE PATH "Directory holding devcfg.csv and its certificates")

if(EXISTS "${DEVCFG_DIR}/devcfg.csv")
    configure_file("${DEVCFG_DIR}/devcfg.csv" "${CMAKE_BINARY_DIR}/devcfg.csv" @ONLY)
    file(GLOB devcfg_files CONFIGURE_DEPENDS "${DEVCFG_DIR}/*")
    nvs_create_partition_image(devcfg "${CMAKE_BINARY_DIR}/devcfg.csv" FLASH_IN_PROJECT DEPENDS ${devcfg_files})
else()
    message(STATUS "No ${DEVCFG_DIR}/devcfg.csv, the devcfg partition will not be flashed")
endif()
