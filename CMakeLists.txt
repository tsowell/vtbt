# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(vtbt)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/beeper.c)
target_sources(app PRIVATE src/bluetooth.c)
target_sources(app PRIVATE src/leds.c)
target_sources(app PRIVATE src/uart.c)
target_sources(app PRIVATE src/lk201_uart.c)
