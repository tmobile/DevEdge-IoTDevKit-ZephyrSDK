# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hello_world)

target_sources(app PRIVATE src/main.c)

target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib)
add_subdirectory(lib)
target_link_libraries(app PUBLIC dfu_gecko)
