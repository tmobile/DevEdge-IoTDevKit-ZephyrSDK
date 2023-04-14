# Copyright (c) 2022 T-Mobile USA, Inc.
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set(ZEPHYR_EXTRA_MODULES "$ENV{ZEPHYR_EXTRA_MODULES};${CMAKE_SOURCE_DIR}/../../")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(tmo_shell)

zephyr_include_directories(${BOARD_DIR})

target_include_directories(app PRIVATE ${ZEPHYR_BASE}/subsys/ PRIVATE ${ZEPHYR_BASE}/drivers/modem/)
target_sources(app PRIVATE src/tmo_shell.c)
target_sources(app PRIVATE src/tmo_buzzer.c)
target_sources(app PRIVATE src/tmo_shell_main.c)
target_sources(app PRIVATE src/tmo_web_demo.c)
target_sources(app PRIVATE src/tmo_http_request.c)
target_sources(app PRIVATE src/tmo_dfu_download.c)
target_sources(app PRIVATE src/tmo_file.c)
target_sources(app PRIVATE src/tmo_modem_edrx.c)
target_sources(app PRIVATE src/tmo_modem_psm.c)
target_sources(app PRIVATE src/buzzer_test.c)
target_sources(app PRIVATE src/led_test.c)
target_sources(app PRIVATE src/misc_test.c)
target_sources(app PRIVATE src/tmo_file.c)
target_sources(app PRIVATE src/tmo_adc.c)
target_sources(app PRIVATE src/tmo_bq24250.c)
target_sources(app PRIVATE src/tmo_battery_ctrl.c)
target_sources(app PRIVATE src/tmo_sntp.c)
target_sources(app PRIVATE src/tmo_modem.c)
target_sources(app PRIVATE src/tmo_tone_player.c)
target_sources_ifdef(CONFIG_WIFI app PRIVATE src/tmo_wifi.c)
target_sources_ifdef(CONFIG_BT_SMP app PRIVATE src/tmo_smp.c)
target_sources_ifdef(CONFIG_BT_PERIPHERAL app PRIVATE src/tmo_ble_demo.c)
target_sources_ifdef(CONFIG_BT_PERIPHERAL app PRIVATE src/tmo_gnss.c)
target_sources_ifdef(CONFIG_NET_SOCKETS_SOCKOPT_TLS app PRIVATE src/tmo_certs.c)
target_sources_ifdef(CONFIG_PING app PRIVATE src/tmo_ping.c)
target_sources_ifdef(CONFIG_TMO_HTTP_MOCK_SOCKET app PRIVATE src/tmo_http_mock_socket.c)
target_sources_ifdef(CONFIG_PM_DEVICE app PRIVATE src/tmo_pm.c)
target_sources_ifdef(CONFIG_PM app PRIVATE src/tmo_pm_sys.c)
target_sources_ifdef(CONFIG_FUEL_GAUGE app PRIVATE src/tmo_fuel_gauge.c)

set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated/)

generate_inc_file_for_target(
    app 
    src/servercert.der
    ${gen_dir}/servercert.der.inc
)

generate_inc_file_for_target(
    app 
    src/devcert.der
    ${gen_dir}/devcert.der.inc
)

generate_inc_file_for_target(
    app 
    src/devkey.der
    ${gen_dir}/devkey.der.inc
)

generate_inc_file_for_target(
    app 
    src/digicert_ca.der
    ${gen_dir}/digicert_ca.der.inc
)

generate_inc_file_for_target(
    app
    src/lets-encrypt-r3.der
    ${gen_dir}/lets-encrypt-r3.der.inc
)

generate_inc_file_for_target(
    app
    src/entrust_g2_ca.der
    ${gen_dir}/entrust_g2_ca.der.inc
)

if(DEFINED CONFIG_TMO_SHELL_BUILD_EK)
	add_subdirectory(src/ek18)
endif()


# zephyr: base upstream version tag used for tmo-main
if(DEFINED ENV{ZEPHYR_BASE_TAG})
	add_compile_definitions(VERSION_ZEPHYR_BASE_TAG="$ENV{ZEPHYR_BASE_TAG}")
else()
	execute_process(
		COMMAND git log --grep "Merge branch 'main' into tmo-main" --max-count 1 --pretty=%H
		WORKING_DIRECTORY ${ZEPHYR_BASE}
		OUTPUT_VARIABLE ZEPHYR_BASE_TAG
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	if ("${ZEPHYR_BASE_TAG}" STREQUAL "")
		execute_process(
			COMMAND git log --grep "^Initial commit$" --max-count 1 --pretty=%H
			WORKING_DIRECTORY ${ZEPHYR_BASE}
			OUTPUT_VARIABLE ZEPHYR_BASE_TAG
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)
	endif()
	if ("${ZEPHYR_BASE_TAG}" STREQUAL "")
		set(ZEPHYR_BASE_TAG "UNKNOWN")
	endif()
	message("ZEPHYR_BASE_TAG: ${ZEPHYR_BASE_TAG}")
	add_compile_definitions(VERSION_ZEPHYR_BASE_TAG="${ZEPHYR_BASE_TAG}")
endif()

# tmo-rtos: current version tag in tmo-main
if(DEFINED ENV{TMO_RTOS_TAG})
	add_compile_definitions(VERSION_TMO_RTOS_TAG="$ENV{TMO_RTOS_TAG}")
else()
	execute_process(
		COMMAND git describe --dirty --tags
		WORKING_DIRECTORY ${ZEPHYR_BASE}
		OUTPUT_VARIABLE RAW_TMO_RTOS_TAG
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	if ("${RAW_TMO_RTOS_TAG}" STREQUAL "")
		set(RAW_TMO_RTOS_TAG "UNKNOWN")
	endif()
	string(REPLACE "-dirty" "+" TMO_RTOS_TAG ${RAW_TMO_RTOS_TAG})
	message("TMO_RTOS_TAG: ${TMO_RTOS_TAG}")
	add_compile_definitions(VERSION_TMO_RTOS_TAG="${TMO_RTOS_TAG}")
endif()

if(DEFINED ENV{SLOT})
	add_compile_definitions(BOOT_SLOT="$ENV{SLOT}")
endif()

# tmo-sdk: current version tag in tmo-sdk
if(DEFINED ENV{TMO_SDK_TAG})
	add_compile_definitions(VERSION_TMO_SDK_TAG="$ENV{TMO_SDK_TAG}")
else()
	execute_process(
		COMMAND git describe --abbrev=0 --tags
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		OUTPUT_VARIABLE LATEST_TMO_TAG
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	execute_process(
		COMMAND git describe --dirty --always --all --long --match ${LATEST_TMO_TAG}
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		OUTPUT_VARIABLE RAW_TMO_SDK_TAG
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	if ("${RAW_TMO_SDK_TAG}" STREQUAL "")
		set(RAW_TMO_SDK_TAG "UNKNOWN")
	endif()
	string(REPLACE "-dirty" "+" TMO_SDK_TAG ${RAW_TMO_SDK_TAG})
	message("TMO_SDK_TAG: ${TMO_SDK_TAG}")
	add_compile_definitions(VERSION_TMO_SDK_TAG="${TMO_SDK_TAG}")
endif()

add_compile_definitions(VERSION_BOARD="${BOARD}")
