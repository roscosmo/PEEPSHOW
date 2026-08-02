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
set(CMAKE_PROJECT_NAME PeepShowV0)

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

# Keep custom sources outside Cube-generated CMake so regen cannot drop them.
target_sources(stm32cubemx INTERFACE
    Core/Src/LS013B7DH05.c
    Core/Src/display_renderer.c
    Core/Src/font8x8_basic.c
)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    Core/Src/ADP5360.c
    Core/Src/TMAG5273.c
    Core/Src/tmag_joy.c
    Core/Src/LIS2DUX12.c
    Core/Src/AT25SL128A.c
    Core/Src/render_demo.c
    Core/Src/ui/ui_router.c
    Core/Src/ui/ui_menu_tree.c
    Core/Src/ui/pages/page_home.c
    Core/Src/ui/pages/page_menu.c
    Core/Src/ui/pages/page_menu_list_common.c
    Core/Src/ui/pages/page_menu_input_sub.c
    Core/Src/ui/pages/page_menu_sensors_sub.c
    Core/Src/ui/pages/page_menu_power_time_sub.c
    Core/Src/ui/pages/page_menu_system_sub.c
    Core/Src/ui/pages/page_joy_cal.c
    Core/Src/ui/pages/page_stub_common.c
    Core/Src/ui/pages/page_batt_stats.c
    Core/Src/ui/pages/page_communications.c
    Core/Src/ui/pages/page_joy_cursor.c
    Core/Src/ui/pages/page_joy_target.c
    Core/Src/ui/pages/page_lis2.c
    Core/Src/ui/pages/page_lis2_steps.c
    Core/Src/ui/pages/page_menu_input.c
    Core/Src/ui/pages/page_rtc_set.c
    Core/Src/ui/pages/page_seed.c
    Core/Src/ui/pages/page_sleep.c
    Core/Src/ui/pages/page_sound.c
    Core/Src/ui/pages/page_storage.c
    Core/Src/ui/pages/page_display_stress.c
)

set_source_files_properties(
    Core/Src/display_renderer.c
    Core/Src/render_demo.c
    Core/Src/ui/pages/page_display_stress.c
    PROPERTIES
    COMPILE_FLAGS "-O3 -ffast-math -fno-math-errno -ffp-contract=fast"
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
