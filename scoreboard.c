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
 * @brief Scoreboard protocol (Implementation)
 */
#include "scoreboard.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"

#include "generated/bitstream.pio.h"

#include "pico/critical_section.h"

#define LOG(fmt, ...) printf("Core %u: " fmt, get_core_num(), ##__VA_ARGS__)

/* Defined in main_controller.c */
[[noreturn]] extern void die(void);

#define arraysize(X) (sizeof(X) / sizeof(X[0]))

uint32_t scoreboard_cmd_buf_element_count = 0;

static uint32_t* scoreboard_cmd_buf_dma;
static uint32_t* _Atomic scoreboard_cmd_buf_pending;
uint32_t* scoreboard_cmd_buf_writing;

static int scoreboard_dma_channel = 0;

static _Atomic bool scoreboard_cmd_buf_swap_requested;
static critical_section_t swap_crit_section;

void __not_in_flash_func(dma_irh)()
{
    dma_hw->ints0 = (1u << scoreboard_dma_channel);
    critical_section_enter_blocking(&swap_crit_section);
    if (atomic_exchange(&scoreboard_cmd_buf_swap_requested, 0))
        scoreboard_cmd_buf_dma = atomic_exchange(&scoreboard_cmd_buf_pending, scoreboard_cmd_buf_dma);
    critical_section_exit(&swap_crit_section);
    dma_channel_set_read_addr(scoreboard_dma_channel, scoreboard_cmd_buf_dma, true);
}

static PIO pio;
static uint sm;
static uint offset;

static void reset_command_buf(uint32_t* cmd_buf, uint32_t num_elements)
{
    uint8_t c = scoreboard_crumb_restart;
    c = (c << 2) | scoreboard_crumb_restart;
    c = (c << 2) | scoreboard_crumb_restart;
    c = (c << 2) | scoreboard_crumb_restart;

    memset(cmd_buf, c, num_elements * sizeof(*cmd_buf));
}

void scoreboard_init(uint32_t dma_irq_num, uint32_t gpio, bool invert, uint32_t cmd_buf_element_count, uint32_t cmd_buf_repeats)
{
    critical_section_init(&swap_crit_section);

    pio_claim_free_sm_and_add_program(&scoreboard_bitstream_tx_program, &pio, &sm, &offset);
    scoreboard_bitstream_tx_program_init(pio, sm, offset, gpio, 1000000);

    gpio_set_outover(gpio, invert ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
    gpio_set_drive_strength(gpio, GPIO_DRIVE_STRENGTH_4MA);
    gpio_set_slew_rate(gpio, GPIO_SLEW_RATE_SLOW);

    /* Check that cmd_buf_element_count is a power of two*/
    if (cmd_buf_element_count & (cmd_buf_element_count - 1))
    {
        LOG("Invalid cmd_buf_element_count: %u\n", cmd_buf_element_count);
        die();
    }

    scoreboard_cmd_buf_element_count = cmd_buf_element_count;

    size_t cmd_buf_size_bytes = cmd_buf_element_count * sizeof(uint32_t);

    uint32_t* scoreboard_cmd_buf_backing0 = aligned_alloc(cmd_buf_size_bytes, cmd_buf_size_bytes * 3);
    uint32_t* scoreboard_cmd_buf_backing1 = scoreboard_cmd_buf_backing0 + cmd_buf_element_count;
    uint32_t* scoreboard_cmd_buf_backing2 = scoreboard_cmd_buf_backing1 + cmd_buf_element_count;

    reset_command_buf(scoreboard_cmd_buf_backing0, cmd_buf_element_count);
    reset_command_buf(scoreboard_cmd_buf_backing1, cmd_buf_element_count);
    reset_command_buf(scoreboard_cmd_buf_backing2, cmd_buf_element_count);

    scoreboard_cmd_buf_dma = scoreboard_cmd_buf_backing0;
    atomic_init(&scoreboard_cmd_buf_pending, scoreboard_cmd_buf_backing1);
    scoreboard_cmd_buf_writing = scoreboard_cmd_buf_backing2;

    atomic_init(&scoreboard_cmd_buf_swap_requested, 0);

    scoreboard_dma_channel = dma_claim_unused_channel(true);

    dma_channel_config pio_dma_chan_config = dma_channel_get_default_config(scoreboard_dma_channel);

    channel_config_set_transfer_data_size(&pio_dma_chan_config, DMA_SIZE_32);
    channel_config_set_read_increment(&pio_dma_chan_config, true);
    channel_config_set_write_increment(&pio_dma_chan_config, false);
    /* Compute log2 of cmd_buf_element_count */
    uint32_t ring_size = 0;
    for (uint32_t i = cmd_buf_element_count * sizeof(uint32_t); i >>= 1; ++ring_size)
        ;
    channel_config_set_ring(&pio_dma_chan_config, false, ring_size);
    channel_config_set_dreq(&pio_dma_chan_config, pio_get_dreq(pio, sm, true));

    dma_channel_configure(scoreboard_dma_channel, //
        &pio_dma_chan_config, //
        &pio->txf[sm], /* PIO TX FIFO */
        scoreboard_cmd_buf_dma, //
        cmd_buf_repeats * cmd_buf_element_count, //
        false);

    dma_irqn_set_channel_enabled(dma_irq_num, scoreboard_dma_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_NUM(dma_irq_num), dma_irh);
    irq_set_enabled(DMA_IRQ_NUM(dma_irq_num), true);
    dma_irh();
}

void scoreboard_swap()
{
    critical_section_enter_blocking(&swap_crit_section);
    scoreboard_cmd_buf_writing = atomic_exchange(&scoreboard_cmd_buf_pending, scoreboard_cmd_buf_writing);
    atomic_store(&scoreboard_cmd_buf_swap_requested, 1);
    critical_section_exit(&swap_crit_section);

    reset_command_buf(scoreboard_cmd_buf_writing, scoreboard_cmd_buf_element_count);
}
