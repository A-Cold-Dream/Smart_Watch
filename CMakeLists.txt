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
set(CMAKE_PROJECT_NAME Smart_Watch)

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

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

set(LVGL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/LVGL)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Compile compact copies in the build tree, keeping the GUI Guider exports intact.
function(watch_pack_font source_list font)
    get_filename_component(name "${font}" NAME)
    set(output "${CMAKE_CURRENT_BINARY_DIR}/packed_fonts/${name}")
    set(packer "${CMAKE_CURRENT_SOURCE_DIR}/cmake/pack_lvgl_font.py")
    add_custom_command(OUTPUT "${output}"
        COMMAND Python3::Interpreter "${packer}" "${font}" "${output}" ${ARGN}
        DEPENDS "${font}" "${packer}"
        VERBATIM)
    set(sources ${${source_list}})
    list(REMOVE_ITEM sources "${font}")
    list(APPEND sources "${output}")
    set(${source_list} ${sources} PARENT_SCOPE)
endfunction()

file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS "${LVGL_DIR}/src/*.c")
watch_pack_font(LVGL_SOURCES "${LVGL_DIR}/src/font/lv_font_montserrat_14_aligned.c")
add_library(lvgl STATIC ${LVGL_SOURCES})
target_include_directories(lvgl PUBLIC ${LVGL_DIR} Config)
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)
# Keep the renderer within the F411's flash budget in Debug builds too.
target_compile_options(lvgl PRIVATE $<$<CONFIG:Debug>:-Os>)

file(GLOB_RECURSE WATCH_UI_SOURCES CONFIGURE_DEPENDS
    "Middlewares/MyUI/generated/*.c"
    "Middlewares/MyUI/custom/*.c"
)
file(GLOB WATCH_FONTS CONFIGURE_DEPENDS "Middlewares/MyUI/generated/assets/fonts/lv_font_*.c")
foreach(font IN LISTS WATCH_FONTS)
    if(font MATCHES "/lv_font_Alatsi_48\\.c$")
        watch_pack_font(WATCH_UI_SOURCES "${font}" --clock)
    else()
        watch_pack_font(WATCH_UI_SOURCES "${font}")
    endif()
endforeach()
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${WATCH_UI_SOURCES}
    APP/Src/Services/watch_display.c
    Components/Display/Src/lcd_init.c
    Components/Display/Src/lcd.c
    Components/Display/Src/CST816.c
    Components/Sensors/Src/AHT21.c
    Components/Sensors/Src/MPU6050.c
    BSP/Src/iic_hal.c
    BSP/Src/Key.c
    BSP/Src/Led.c
    System/delay.c
    
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    APP/Inc
    BSP/Inc
    Components/Display/Inc
    Components/Sensors/Inc
    Middlewares/MyUI/custom
    Middlewares/MyUI/generated
    System
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    lvgl
)
