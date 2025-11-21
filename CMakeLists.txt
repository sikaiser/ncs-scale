cmake_minimum_required(VERSION 3.20.0)

set(SHIELD ssd1306_128x32) # needs to be before find_package

set(ZEPHYR_EXTRA_MODULES "${CMAKE_SOURCE_DIR}/modules/HX711")
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(ncs-scale)

target_sources(app PRIVATE
    src/main.c
    src/channels.c
    src/weight.c
    src/bluetooth.c
    src/display.c
)

target_include_directories(app PRIVATE
    include
)
