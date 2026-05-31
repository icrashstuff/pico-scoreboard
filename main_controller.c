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
 * @brief Controller program entry point
 */
#include <stdio.h>

#include "pico/stdlib.h"

#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "hardware/watchdog.h"

#include "generated/bitstream.pio.h"

/**
 * @brief GPIO pin to output scoreboard controller signal on
 *
 * This connects to a simple line driver consisting of a 2N2907 PNP transistor with the following connections:\n
 * - Base: 68 ohm resistor to 5.25 volt supply
 * - Base: 330 ohm resistor to GPIO_SIGNAL (rp2350 pins are 5.5 volt tolerant)
 * - Emitter: Direct connection to 5.25 volt supply
 * - Collector: Direct connection to BNC signal wire
 * - Collector: 100 ohm resistor to GROUND
 */
#define GPIO_SIGNAL 22

/**
 * Invert scoreboard controller signal
 */
#define SIGNAL_INVERTED 1

#define LOG(fmt, ...) printf("Core %u: " fmt, get_core_num(), ##__VA_ARGS__)

[[noreturn]] void die(void)
{
    const uint64_t us_up = time_us_64();
    LOG("die() called @ %llu.%06llus\n", us_up / 1000000ull, us_up % 1000000ull);
    fflush(stdout);
    watchdog_enable(0, false);
    while (1)
        tight_loop_contents();
}

int main()
{
#ifdef PICO_DEFAULT_LED_PIN
    gpio_set_function(PICO_DEFAULT_LED_PIN, GPIO_FUNC_PWM);
    pwm_set_wrap(pwm_gpio_to_slice_num(PICO_DEFAULT_LED_PIN), 4);
    pwm_set_gpio_level(PICO_DEFAULT_LED_PIN, 3);
    pwm_set_enabled(pwm_gpio_to_slice_num(PICO_DEFAULT_LED_PIN), true);
#endif

    stdio_init_all();
    while (!stdio_usb_connected() && time_us_64() < 5000000ull)
        sleep_ms(5);

    putc('\n', stdout);
    putc('\n', stdout);
    LOG("Begin boot\n");
    LOG("===> Program info\n");
    LOG("Program name:  pico-scoreboard-controller\n");
    LOG("Compile date:  %s %s\n", __DATE__, __TIME__);
    LOG("PICO_BOARD:    %s\n", CMAKE_PICO_BOARD);
    LOG("PICO_PLATFORM: %s\n", CMAKE_PICO_PLATFORM);
    putc('\n', stdout);
    LOG("===> USB config\n");
    LOG("Manufacturer: \"%s\"\n", USBD_MANUFACTURER);
    LOG("Product:      \"%s\"\n", USBD_PRODUCT);
    putc('\n', stdout);
    putc('\n', stdout);

    PIO pio;
    uint sm;
    uint offset;

    pio_claim_free_sm_and_add_program(&scoreboard_bitstream_tx_program, &pio, &sm, &offset);
    scoreboard_bitstream_tx_program_init(pio, sm, offset, GPIO_SIGNAL, 1000000);

    gpio_set_outover(GPIO_SIGNAL, SIGNAL_INVERTED ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
    gpio_set_drive_strength(GPIO_SIGNAL, GPIO_DRIVE_STRENGTH_4MA);
    gpio_set_slew_rate(GPIO_SIGNAL, GPIO_SLEW_RATE_SLOW);

    const uint8_t crumb_restart = 0x3;
    const uint8_t crumb_bit1 = 0x2;
    const uint8_t crumb_bit0 = 0x0;

    LOG("Setup done, beginning loop\n");
    while (1)
    {
        char bitstream[] =
#define CMD(A, B, C, D) A B C D
            CMD("r111", "0000", "1111", "1111") // CMD: 0x0
            CMD("1111", "1000", "1111", "1111") // CMD: 0x1
            CMD("1111", "0100", "1100", "1111") // CMD: 0x2
            CMD("1111", "1100", "0000", "1111") // CMD: 0x3
            CMD("1111", "0010", "1111", "1111") // CMD: 0x4
            CMD("1111", "1110", "0000", "1111") // CMD: 0x7
            CMD("1111", "0110", "0000", "1111") // CMD: 0x6
            CMD("1111", "1010", "0000", "1111") // CMD: 0x5
            CMD("1111", "0001", "0000", "1111") // CMD: 0x8
            CMD("1111", "1001", "1111", "1111") // CMD: 0x9
            CMD("1111", "0101", "1111", "1111") // CMD: 0xA
            CMD("1111", "1101", "1111", "1111") // CMD: 0xB
            CMD("1111", "0011", "1111", "1111") // CMD: 0xC
            CMD("1111", "1011", "1111", "1111") // CMD: 0xD
            CMD("1111", "0111", "1111", "1111") // CMD: 0xE
            CMD("1111", "1111", "1111", "1111") // CMD: 0xF
            ;
#undef CMD

        uint32_t t = (time_us_32() / (1000 * 100)) % 1800;
        uint32_t ts = t % 60;
        uint32_t tm = t / 60;

        uint32_t ts0 = ts % 10;
        uint32_t ts1 = ts / 10;

        uint32_t tm0 = tm % 10;
        uint32_t tm1 = tm / 10;

        /* timer_digit_0: offset=0x57 size=4 */
        bitstream[0x57 + 1] = (ts0 & 1) ? '1' : '0';
        bitstream[0x58 + 1] = (ts0 & 2) ? '1' : '0';
        bitstream[0x59 + 1] = (ts0 & 4) ? '1' : '0';
        bitstream[0x5A + 1] = (ts0 & 8) ? '1' : '0';

        /* timer_digit_1: offset=0x67 size=3
         * While not used by the controller this digit does have the logic to display the numbers 6 and 7
         * This digit does not have the logic to display the number 8, but you can fake it by switching between two values very rapidly
         * This digit does not have the logic to display the number 9, but you can fake it by switching between two values very rapidly
         */
        bitstream[0x67 + 1] = (ts1 & 1) ? '1' : '0';
        bitstream[0x68 + 1] = (ts1 & 2) ? '1' : '0';
        bitstream[0x69 + 1] = (ts1 & 4) ? '1' : '0';

        /* timer_digit_2: offset=0x77 size=4 */
        bitstream[0x77 + 1] = (tm0 & 1) ? '1' : '0';
        bitstream[0x78 + 1] = (tm0 & 2) ? '1' : '0';
        bitstream[0x79 + 1] = (tm0 & 4) ? '1' : '0';
        bitstream[0x7A + 1] = (tm0 & 8) ? '1' : '0';

        /* timer_digit_3: offset=0x87 size=2
         * This digit does not have the logic to display the number 3, attempt to do so will display both 1 and 2 at the same time
         */
        bitstream[0x87 + 1] = (tm1 & 1) ? '1' : '0';
        bitstream[0x88 + 1] = (tm1 & 2) ? '1' : '0';

        uint32_t word = 0;
        for (size_t i = 0, p = 0; i < sizeof(bitstream); i++)
        {
            switch (bitstream[i])
            {
            case 'r':
                word = (word << 2) | crumb_restart;
                p++;
                break;
            case '0':
                word = (word << 2) | crumb_bit0;
                p++;
                break;
            case '1':
                word = (word << 2) | crumb_bit1;
                p++;
                break;
            default:
                break;
            }
            if (p == 16)
            {
                pio_sm_put_blocking(pio, sm, word);
                p = 0;
            }
        }
        pwm_set_gpio_level(PICO_DEFAULT_LED_PIN, ((time_us_32() >> 18) & 1) ? 3 : 0);
    }
}
