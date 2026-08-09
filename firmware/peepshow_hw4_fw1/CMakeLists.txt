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

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

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
    Core/Src/render_font.c
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
    Core/Src/audio_assets.c
    Core/Src/game_map.c
    Core/Src/game_tileset.c
    Core/Src/game_sprite_anim.c
    Core/Src/game_package.c
    Core/Src/game_package_manifest.c
    Core/Src/game_runtime.c
    Core/Src/game_mode_topdown_basic.c
    Core/Src/render_demo.c
    Core/Src/render_demo_3d_walk.c
    Core/Src/render_demo_title_anim.c
    Core/Src/ui/ui_runtime_context.c
    Core/Src/ui/ui_router.c
    Core/Src/ui/ui_page_registry.c
    Core/Src/ui/ui_menu_root.c
    Core/Src/ui/ui_menu_system.c
    Core/Src/ui/ui_menu_input.c
    Core/Src/ui/ui_menu_sensors.c
    Core/Src/ui/ui_menu_power_time.c
    Core/Src/ui/ui_menu_usb_flash_prompt.c
    Core/Src/ui/ui_page_home.c
    Core/Src/ui/ui_page_pet.c
    Core/Src/ui/ui_page_batt_stats.c
    Core/Src/ui/ui_page_audio_levels.c
    Core/Src/ui/ui_page_lis2.c
    Core/Src/ui/ui_page_lis2_steps.c
    Core/Src/ui/ui_page_joy_common.c
    Core/Src/ui/ui_page_joy_target.c
    Core/Src/ui/ui_page_joy_cal.c
    Core/Src/ui/ui_page_flashing.c
)

set_source_files_properties(
    Core/Src/display_renderer.c
    Core/Src/game_mode_topdown_basic.c
    Core/Src/LS013B7DH05.c
    Core/Src/render_demo.c
    Core/Src/render_demo_3d_walk.c
    Core/Src/render_demo_title_anim.c
    PROPERTIES
    COMPILE_OPTIONS "$<$<CONFIG:Debug>:-O3>"
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

