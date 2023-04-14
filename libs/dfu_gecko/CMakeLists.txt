cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_sources_ifdef(CONFIG_DFU_GECKO_LIB app PRIVATE dfu_gecko_lib.c)
