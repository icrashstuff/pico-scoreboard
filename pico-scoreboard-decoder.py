#!/bin/python3
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
# @brief Simple analyzer program that decodes the signaling protocol used by the
#        Nevco MC-2 scoreboard controller and prints the state as a series of bits
import re
import sys
import argparse
import textwrap

def process_vcd(fd,
                invert_samples=False,
                unique_frames_limit=0,
                frame_size_filter=0,
                verbose=False):
    for i in fd:
        if (i.startswith("$enddefinitions")):
            break
        m = re.match(r'^\$timescale (?P<mag>\d+) (?P<unit>[\S]+)', i)
        if (m is None):
            continue
        units = {
            "us": 1,
            "ns": 1000,
            "ps": 1000 * 1000,
            "fs": 1000 * 1000 * 1000,
        }
        if (m.groupdict()["unit"] not in units):
            raise Exception(f"Unsuitable unit {m.groups.unit}")

        unit_multiplier = units[m.groupdict()["unit"]
                                ] // int(m.groupdict()["mag"])

        jitter = unit_multiplier * 3 // 4
        time_high_max_for_bit0 = 1 * unit_multiplier + jitter
        time_high_max_for_bit1 = 3 * unit_multiplier + jitter
        time_high_for_restart = 4 * unit_multiplier

    if (verbose):
        print(f"unit_multiplier={unit_multiplier}")
        print(f"jitter={jitter}")
        print(f"time_high_max_for_bit1={time_high_max_for_bit1}")
        print(f"time_high_max_for_bit0={time_high_max_for_bit0}")
        print(f"time_high_for_restart={time_high_for_restart}")

    last_joined_buf = ""
    last_level = None
    last_rise = 0
    last_fall = 0
    number_of_unique_frames = 0
    buf = []
    for i in fd:
        m = re.match(r'#\s*(?P<samplenum>\d+) \s*(?P<level>\d)(?P<id>\S)', i)
        if (m is None):
            continue
        samplenum = int(m.groupdict()["samplenum"])
        if (invert_samples):
            level = not int(m.groupdict()["level"])
        else:
            level = int(m.groupdict()["level"])
        del m
        if (last_level is None):
            last_level = level
            continue

        if (last_level == 0 and level == 1):  # Rising edge
            last_rise = samplenum
        elif (last_level == 1 and level == 0):  # Falling edge
            last_fall = samplenum
        else:  # No change
            last_level = level
            continue
        last_level = level

        if (last_fall <= last_rise):  # We don't process on rising edges
            continue
        high_period = last_fall - last_rise

        if (high_period <= time_high_max_for_bit0):
            buf.append(0)
        elif (high_period <= time_high_max_for_bit1):
            buf.append(1)
        else:
            # Convert buffer to string of 0's and 1's
            s = "".join(["1" if i else "0" for i in buf])
            # Convert s to array of 8 character long string
            s = [s[i:i+8] for i in range(0, len(s), 8)]
            # Join the array 's' with space a the separator
            joined_buf = " ".join(s)

            if (frame_size_filter <= 0 or len(buf) == frame_size_filter):
                if (last_joined_buf != joined_buf):
                    print(f"New state (len={len(buf)}): {joined_buf}")
                    number_of_unique_frames += 1

            if (unique_frames_limit > 0 and number_of_unique_frames >= unique_frames_limit):
                return

            last_joined_buf = joined_buf

            buf = []

            if (high_period - time_high_for_restart <= time_high_max_for_bit0):
                buf.append(0)
            elif (high_period - time_high_for_restart <= time_high_max_for_bit1):
                buf.append(1)


def main(*args, **kwargs):
    parser = argparse.ArgumentParser(
        description=textwrap.dedent("""
            Simple analyzer program that decodes the signaling protocol used by the
            Nevco MC-2 scoreboard controller and prints the state as a series of bits.

            Note: this script was written to parse single channel Value Change Dump (VCD) files
            emitted by sigrok-cli, it probably won't successfully parse VCD files from other programs.
        """),
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="Example: `%(prog)s dump.vcd`\n"
               "Example: `sigrok-cli -d fx2lafw --config samplerate=\"4 Mhz\" --continuous -C D0 -O vcd | %(prog)s`\n"
               "Example: `sigrok-cli -i session.sr -C D0 -O vcd | %(prog)s`\n"
    )
    parser.add_argument(
        "sigrok_vcd_file",
        help="Value Change Dump file to decode. If no filename is specified, standard input will be read instead.",
        nargs="?",
        default=None,
    )
    parser.add_argument(
        "--invert",
        help="Invert signal (Useful if sampling from certain points on the circuit board)",
        default=False,
        action="store_true"
    )
    parser.add_argument(
        "--frame-size",
        metavar="N",
        help="Only print signal frames of size N (0 to disable)",
        default=0,
        type=int
    )
    parser.add_argument(
        "--num-frames",
        metavar="N",
        help="Only print N number of unique signal frames (0 to disable)",
        default=0,
        type=int
    )
    parser.add_argument(
        "-v", "--verbose",
        help="Print various debugging information",
        default=False,
        action="store_true"
    )
    args = parser.parse_args(*args, **kwargs)

    keywords_for_process = {
        "invert_samples": args.invert,
        "frame_size_filter": args.frame_size,
        "unique_frames_limit": args.num_frames,
        "verbose": args.verbose
    }

    if (args.sigrok_vcd_file is None):
        process_vcd(sys.stdin, **keywords_for_process)
    else:
        with open(args.sigrok_vcd_file, 'r') as fd:
            process_vcd(fd, **keywords_for_process)


if __name__ == "__main__":
    main()
