# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

target_include_directories(app PRIVATE src/)
target_sources(app PRIVATE src/zephyrio.c src/kermit_cmd.c src/kermit.c)