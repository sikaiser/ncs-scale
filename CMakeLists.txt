cmake_minimum_required(VERSION 3.20.0)

set(ZEPHYR_EXTRA_MODULES "${CMAKE_SOURCE_DIR}/modules/HX711")
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(ncs-scale)

target_sources(app PRIVATE
    src/main.c
    src/channels.c
    src/weight.c
    src/bluetooth.c
    src/bluetooth_protocol.c
)

if(CONFIG_DISPLAY)
    target_sources(app PRIVATE src/display.c)
endif()

target_include_directories(app PRIVATE
    include
)
