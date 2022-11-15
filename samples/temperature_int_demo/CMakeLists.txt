# Copyright (c) 2022 T-Mobile USA, Inc.
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(temp_demo)

target_include_directories(app PRIVATE ${ZEPHYR_BASE}/subsys/ )
FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE src/temperature_int_demo.c)

set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated/)

add_compile_definitions(VERSION_BOARD="${BOARD}")
