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
set(CMAKE_PROJECT_NAME peepshow_hw6_fw0)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Re-apply non-Cube invariants after regeneration.
get_target_property(_stm32cubemx_defs stm32cubemx INTERFACE_COMPILE_DEFINITIONS)
if(NOT _stm32cubemx_defs OR _stm32cubemx_defs STREQUAL "_stm32cubemx_defs-NOTFOUND")
    set(_stm32cubemx_defs "")
endif()
list(FILTER _stm32cubemx_defs EXCLUDE REGEX "^TX_PORT_BASEPRI=.*")
list(APPEND _stm32cubemx_defs TX_PORT_BASEPRI=0x20)
set_target_properties(stm32cubemx PROPERTIES
    INTERFACE_COMPILE_DEFINITIONS "${_stm32cubemx_defs}"
)

# The display owner builds and links the runtime LPBAM queue directly.  Keep
# CubeMX's scenario scaffold in the project, but do not compile its alternate
# peripheral initializer alongside the normal main.c initializers.
get_target_property(_stm32cubemx_sources stm32cubemx INTERFACE_SOURCES)
list(FILTER _stm32cubemx_sources EXCLUDE REGEX
    "lpbam_lpbamap1_scenario_config\\.c$"
)
set_target_properties(stm32cubemx PROPERTIES
    INTERFACE_SOURCES "${_stm32cubemx_sources}"
)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Src/LS013B7DH05.c
    Core/Src/display_renderer.c
    Core/Src/ps_lpbam_display_buffers.c
    LPBAM/LpbamAp1/lpbam_lpbamap1_scenario_build.c
    Core/Src/ps_dev_at25sl128a.c
    Core/Src/ps_dev_audio.c
    Core/Src/ps_dev_adp5360.c
    Core/Src/ps_dev_lis2dux12.c
    Core/Src/ps_dev_tmag3001.c
    Core/Src/ps_hw_i2c3.c
    Core/Src/ps_input_buttons.c
    Core/Src/ps_input_joystick.c
    Core/Src/ps_ui_router.c
    Core/Src/ps_hw6_trace.c
    Core/Src/ps_hw6_clock_policy.c
    Core/Src/ps_hw6_owner_services.c
    Core/Src/ps_hw6_owner_state_machines.c
    Core/Src/ps_hw6_peripheral_probe.c
    Core/Src/ps_hw6_rtos_probe.c
    Core/Src/ps_hw6_usb_export.c
    Core/Src/ps_storage_flash_block.c
    Core/Src/ps_storage_filex_levelx.c
    Core/Src/ps_storage_layout.c
    Core/Src/ps_storage_msc_bridge.c
    Drivers/Components/lis2dux12/lis2dux12_reg.c
    Utilities/lpbam/stm32_adv_lpbam_common.c
    Utilities/lpbam/STM32U5/stm32_ll_lpbam.c
    Utilities/lpbam/stm32_lpbam_common.c
    Utilities/lpbam/stm32_adv_lpbam_spi.c
    Utilities/lpbam/stm32_lpbam_spi.c
    Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_callback.c
    Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_endpoint_create.c
    Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_endpoint_destroy.c
    Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_endpoint_reset.c
    Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_endpoint_stall.c
    Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_endpoint_status.c
    Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_frame_number_get.c
    Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_function.c
    Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_initialize.c
    Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_initialize_complete.c
    Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_interrupt_handler.c
    Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_transfer_abort.c
    Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_transfer_request.c
    Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_transfer_run.c
    Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_uninitialize.c
)

set_source_files_properties(
    Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_callback.c
    Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_frame_number_get.c
    USBX/App/ux_device_descriptors.c
    PROPERTIES COMPILE_OPTIONS -Wno-unused-parameter
)
# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    Drivers/Components/lis2dux12
    LPBAM/LpbamAp1
    Utilities/lpbam
    Utilities/lpbam/STM32U5
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
