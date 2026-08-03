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

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Src/LS013B7DH05.c
    Core/Src/ps_dev_at25sl128a.c
    Core/Src/ps_dev_audio.c
    Core/Src/ps_dev_ls013b7dh05.c
    Core/Src/ps_dev_adp5360.c
    Core/Src/ps_dev_lis2dux12.c
    Core/Src/ps_dev_tmag3001.c
    Core/Src/ps_hw_i2c3.c
    Core/Src/ps_hw6_owner_services.c
    Core/Src/ps_hw6_owner_state_machines.c
    Core/Src/ps_hw6_peripheral_probe.c
    Core/Src/ps_hw6_rtos_probe.c
    Core/Src/ps_storage_flash_block.c
    Core/Src/ps_storage_filex_levelx.c
    Core/Src/ps_storage_layout.c
    Drivers/Components/lis2dux12/lis2dux12_reg.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    Drivers/Components/lis2dux12
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # CubeMX emits the hooks but does not pass this define to the GNU port ASM.
    TX_LOW_POWER
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
