# Copyright (c) 2022 T-Mobile USA, Inc.
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(wake_sleep)

# TODO: Remove the following after the stderror.h workacound is addressed/removed.
#
# add_definitions(-include /Users/john/Work/zephyrproject-rebase/DevEdge-IoTDevKit-ZephyrSDK/samples/wake_sleep/src/stderror.h)
# target_precompile_headers(app
# #   PUBLIC
# #     project_header.h
#   PRIVATE
#     [["src/stderror.h"]]
#     <unordered_map>
# )

# TODO: ...additionally, uncomment the following and remove the subquent two lines
# target_sources(app PRIVATE "src/main.c")
FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
