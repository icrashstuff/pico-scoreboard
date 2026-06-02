/**
 * pico-scoreboard
 *
 * @file
 * @copyright
 * @parblock
 * SPDX-License-Identifier: MIT
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ian Hangartner <icrashstuff at outlook dot com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * @endparblock
 *
 * @brief Scoreboard protocol (Interface)
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * Requires:
 * - 1x DMA channel (Date rate: ~62.5KB/s)
 * - 1x PIO State machine
 * - 1x DMA_IRQ
 *
 * @param dma_irq_num Which DMA_IRQ_* to use, if in doubt use `(NUM_DMA_IRQS - 1)`
 * @param gpio GPIO pin to emit signal on
 * @param invert Invert signal emitted
 * @param cmd_buf_element_count Number of elements in a command buffer (MUST BE A POWER OF TWO)
 * @param cmd_buf_repeats Number of times to display a command buffer before checking for a new buffer (To reduce IRQ frequency)
 */
void scoreboard_init(uint32_t dma_irq_num, uint32_t gpio, bool invert, uint32_t cmd_buf_element_count, uint32_t cmd_buf_repeats);

/**
 * @brief Swaps @ref scoreboard_cmd_buf_writing with the internal queue and signals for presentation
 *
 * The contents of @ref scoreboard_cmd_buf_writing will be reset to the default sequence
 *
 * This uses an algorithm similar to the Vulkan mailbox present mode.
 */
void scoreboard_swap();

static const uint8_t scoreboard_crumb_restart = 0x3;
static const uint8_t scoreboard_crumb_bit1 = 0x2;
static const uint8_t scoreboard_crumb_bit0 = 0x0;

/**
 * Current command buffer for writing to
 *
 * When finished writing to buffer, call @ref scoreboard_swap()
 **/
extern uint32_t* scoreboard_cmd_buf_writing;

/**
 * Number of elements in a scoreboard command buffer
 */
extern uint32_t scoreboard_cmd_buf_element_count;
