cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME Nucleo_H753ZI_DWS3000)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Vendored Qorvo DW3_QM33_SDK (Drivers/API layer) - repo_root/DW3_QM33_SDK_1.1.1/Drivers
set(DW3_SDK_DRIVERS_PATH ${CMAKE_SOURCE_DIR}/../../DW3_QM33_SDK_1.1.1/Drivers)
set(UWB_DRIVER_PATH ${DW3_SDK_DRIVERS_PATH}/API/Shared/dwt_uwb_driver)
set(EXAMPLES_PATH ${DW3_SDK_DRIVERS_PATH}/API/Src)

# This board only ever talks to a DW3000-series chip (DWS3000 shield)
set(DWT_DW3000 1)
set(DWT_DW3720 0)
add_subdirectory(${UWB_DRIVER_PATH} ${CMAKE_BINARY_DIR}/uwb_driver)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Platform glue (adapted from Qorvo's STM_Nucleo_F429 reference for this board's pinout)
    platform/deca_mutex.c
    platform/deca_probe_interface.c
    platform/deca_sleep.c
    platform/deca_spi.c
    platform/port.c

    # Shared example plumbing
    ${EXAMPLES_PATH}/config_options.c
    ${EXAMPLES_PATH}/examples/examples_info/example_info.c
    ${EXAMPLES_PATH}/examples/shared_data/shared_functions.c

    # DW3000 example(s) selected via config/example_selection.h (this board = SS-TWR responder)
    ${EXAMPLES_PATH}/examples/ex_00a_reading_dev_id/read_dev_id.c
    ${EXAMPLES_PATH}/examples/ex_06b_ss_twr_responder/ss_twr_responder.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    platform
    # 'config' first so our local example_selection.h shadows the vendored default
    config
    ${EXAMPLES_PATH}
    ${EXAMPLES_PATH}/examples/examples_info
    ${EXAMPLES_PATH}/examples/shared_data
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    USE_DRV_DW3000
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    uwb_driver
)

# nano.specs strips float support from printf/snprintf by default; needed if this board
# ever runs an example (e.g. an initiator role) that formats results with %f.
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE -u _printf_float)
