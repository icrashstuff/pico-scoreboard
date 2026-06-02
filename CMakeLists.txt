# pico-scoreboard
#
# @file
# @copyright
# @parblock
# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Ian Hangartner <icrashstuff at outlook dot com>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# @endparblock
#
# @brief Entry CMakeLists.txt
cmake_minimum_required(VERSION 3.13...4.0)

# set(CMAKE_BUILD_TYPE RELEASE)

include(pico_sdk_import.cmake)

project(my_project C CXX ASM)

set (CMAKE_C_STANDARD 11)
set (CMAKE_CXX_STANDARD 11)

pico_sdk_init()

# BEGIN: Common options
function(common_options target)
    target_compile_definitions(${target} PUBLIC
        PICO_DEBUG_MALLOC=1
        USBD_MANUFACTURER="Ian Hangartner"
        USBD_PRODUCT="${target}"
    )
    target_compile_definitions(${target} PRIVATE
        CMAKE_PICO_BOARD="${PICO_BOARD}"
        CMAKE_PICO_PLATFORM="${PICO_PLATFORM}"
    )
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
    target_compile_options(${target} PUBLIC -Wall -Wextra -Wshadow)
    target_link_options(${target} PRIVATE "-Wl,--print-memory-usage")

    if (TARGET tinyusb_device)
        pico_enable_stdio_usb(${target} 1)
        pico_enable_stdio_uart(${target} 1)
    else()
        message(FATAL_ERROR "USB not enabled!")
    endif()

    pico_add_extra_outputs(${target})
endfunction()
# END: Common options

# BEGIN: Controller
add_executable(pico-scoreboard-controller
    main_controller.c
    scoreboard.c
)

target_link_libraries(pico-scoreboard-controller PRIVATE
    pico_sync
    pico_stdlib
    pico_malloc
    hardware_pio
    hardware_pwm
    hardware_dma
    hardware_watchdog
)
pico_generate_pio_header(pico-scoreboard-controller
    ${CMAKE_CURRENT_LIST_DIR}/bitstream.pio
    OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/generated
)
common_options(pico-scoreboard-controller)
# END: Controller
