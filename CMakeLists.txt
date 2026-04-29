#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(peripheral_uart)

# NORDIC SDK APP START
target_sources(app PRIVATE
  src/main.c
  src/ble_conf/ble.c
  src/ble_services/custom_service.c
  src/drv/uart.c
  src/drv/gpio.c
  src/drv/i2c_drv.c
  src/sensor/M117Z/m117_mts01_i2c.c
)

target_include_directories(app PRIVATE 
  ./src
  ./src/ble_conf
  ./src/ble_services
  ./src/drv
  ./src/dfu
  ./src/flash
  ./src/sensor/M117Z
)
# NORDIC SDK APP END
