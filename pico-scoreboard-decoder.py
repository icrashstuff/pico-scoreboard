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
#        Nevco 2-MC scoreboard controller and prints the state as a series of bits
import re
import sys
import typing
import argparse
import textwrap

description = """
Simple analyzer program that decodes the signaling protocol used by the
Nevco 2-MC scoreboard controller and prints the state as a series of bits.

Note: Not all Nevco 2-MC model controllers are the same, the one used
in the creation of this program has the following controls:
- Home score: 100's place, 10's place, 1's place, Bonus
- Guest score: 100's place, 10's place, 1's place, Bonus
- Timer: Reset to zero, Time in/out, Reset,
         10's place (Limited to 0,1,2), 1's place, Up/Down
- Horn: Press to sound

Note: Value Change Dump (VCD) support is limited to single channel dumps emitted
by sigrok-cli, it probably won't successfully parse VCD files from other programs.

This program/project is licensed under the MIT license.
Copyright (c) 2026 Ian Hangartner <icrashstuff at outlook dot com>
See source code or LICENSE.txt for details.
"""
__doc__ = description


class decoder_t:

    def __init__(self,
                 time_units_per_us: int,
                 invert_samples: bool = False,
                 unique_frames_limit: int = 0,
                 frame_size_filter: int = 0,
                 verbose: bool = False):
        self.last_joined_buf = ""
        self.last_level = 0
        self.last_rise = 0
        self.last_fall = 0
        self.number_of_unique_frames = 0

        jitter = time_units_per_us * 3 // 4
        self.time_high_max_for_bit0 = 1 * time_units_per_us + jitter
        self.time_high_max_for_bit1 = 3 * time_units_per_us + jitter
        self.time_high_for_restart = 4 * time_units_per_us

        self.invert_samples = invert_samples
        self.unique_frames_limit = unique_frames_limit
        self.frame_size_filter = frame_size_filter
        self.verbose = verbose

        if (verbose):
            print(f"time_units_per_us={time_units_per_us}")
            print(f"jitter={jitter}")
            print(f"time_high_max_for_bit1={self.time_high_max_for_bit1}")
            print(f"time_high_max_for_bit0={self.time_high_max_for_bit0}")
            print(f"time_high_for_restart={self.time_high_for_restart}")

        self.buf = []

    def feed(self, time_unit: int, level: bool):
        if (self.invert_samples):
            level = not level

        if (self.last_level == 0 and level == 1):  # Rising edge
            self.last_rise = time_unit
        elif (self.last_level == 1 and level == 0):  # Falling edge
            self.last_fall = time_unit
        else:  # No change
            self.last_level = level
            return False
        self.last_level = level

        if (self.last_fall <= self.last_rise):  # We don't process on rising edges
            return False
        high_period = self.last_fall - self.last_rise

        if (high_period <= self.time_high_max_for_bit0):
            self.buf.append(0)
        elif (high_period <= self.time_high_max_for_bit1):
            self.buf.append(1)
        else:
            # Convert buffer to string of 0's and 1's
            s = "".join(["1" if i else "0" for i in self.buf])
            # Convert s to array of 8 character long string
            s = [s[i:i+8] for i in range(0, len(s), 8)]
            # Join the array 's' with space a the separator
            joined_buf = " ".join(s)

            if (self.frame_size_filter <= 0 or len(self.buf) == self.frame_size_filter):
                if (self.last_joined_buf != joined_buf):
                    print(f"New state (len={len(self.buf)}): {joined_buf}")
                    self.number_of_unique_frames += 1

            if (self.unique_frames_limit > 0 and self.number_of_unique_frames >= self.unique_frames_limit):
                return True

            self.last_joined_buf = joined_buf

            self.buf = []

            if (high_period - self.time_high_for_restart <= self.time_high_max_for_bit0):
                self.buf.append(0)
            elif (high_period - self.time_high_for_restart <= self.time_high_max_for_bit1):
                self.buf.append(1)


def process_sigrok_vcd(fd: typing.IO, **kwargs):
    found_timescale = False
    found_wire = False
    for i in fd:
        if (i.startswith("$enddefinitions")):
            break

        m_ts = re.match(r'^\$timescale (?P<mag>\d+) (?P<unit>[\S]+)', i)
        if (m_ts):
            units = {
                "us": 1,
                "ns": 1_000,
                "ps": 1_000_000,
                "fs": 1_000_000_000,
            }
            if (m_ts.groupdict()["unit"] not in units):
                raise Exception(f"Unsuitable unit {m_ts.groupdict()["unit"]}")

            unit_multiplier = units[m_ts.groupdict()["unit"]
                                    ] // int(m_ts.groupdict()["mag"])
            found_timescale = True

        if (i.startswith("$var wire")):
            if (found_wire):
                raise Exception(f"Only signal channel VCD files are accepted!")
            found_wire = True

    if (not found_timescale):
        raise Exception("Unable to get timescale metadata")
    if (not found_wire):
        raise Exception("Unable to get channel metadata")

    decoder = decoder_t(unit_multiplier, **kwargs)

    for i in fd:
        m = re.match(r'#\s*(?P<samplenum>\d+) \s*(?P<level>\d)(?P<id>\S)', i)
        if (m is None):
            continue
        if (decoder.feed(int(m.groupdict()["samplenum"]), int(m.groupdict()["level"]))):
            return


def process_sigrok_bits(fd: typing.IO, **kwargs):
    for i in fd:
        m = re.match(r'Acquisition with '
                     r'(?P<num_enabled_channels>\d+)'
                     r'\/(?P<enabled_channels>\d+) '
                     r'channels at '
                     r'(?P<mag>[\d\.]+) '
                     r'(?P<unit>\S+)', i)
        if (m is None):
            continue
        units = {
            "MHz": 1,
            "GHz": 1_000,
            "THz": 1_000_000,
            "PHz": 1_000_000_000,
        }
        if (m.groupdict()["unit"] not in units):
            raise Exception(f"Unsuitable unit {m.groupdict()["unit"]}")
        if (int(m.groupdict()["num_enabled_channels"]) != 1):
            raise Exception(
                f"Only signal channel sigrok bit files are accepted!")

        unit_multiplier = units[m.groupdict()["unit"]] * \
            int(m.groupdict()["mag"]) * 4
        break

    decoder = decoder_t(unit_multiplier, **kwargs)
    time_unit = 0
    reading_data = False
    while 1:
        char = fd.read(1)
        if not char:
            break
        if (not reading_data):
            if (char == ":"):
                reading_data = True
            continue
        if (char == "0"):
            decoder.feed(time_unit, 0)
            time_unit += 4
        elif (char == "1"):
            decoder.feed(time_unit, 1)
            time_unit += 4
        elif (char == "\n"):
            reading_data = False
        elif (char == "\r"):
            reading_data = False


def main(*args, **kwargs):
    parser = argparse.ArgumentParser(
        description=textwrap.dedent(__doc__),
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="Example: `%(prog)s sigrok_vcd dump.vcd`\n"
               "Example: `%(prog)s sigrok_bits dump.bits`\n"
               "Example: `sigrok-cli -d fx2lafw --config samplerate=\"4 Mhz\" --continuous -C D0 -O vcd | %(prog)s sigrok_vcd`\n"
               "Example: `sigrok-cli -i session.sr -C D0 -O bits | %(prog)s sigrok_bits`\n"
    )
    parser.add_argument(
        "format",
        help="Input file format",
        choices=["sigrok_vcd", "sigrok_bits"],
    )
    parser.add_argument(
        "filename",
        help="File to decode. If no filename is specified, standard input will be read instead.",
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

    if (args.format == "sigrok_vcd"):
        process_function = process_sigrok_vcd
    elif (args.format == "sigrok_bits"):
        process_function = process_sigrok_bits

    if (args.filename is None):
        process_function(sys.stdin, **keywords_for_process)
    else:
        with open(args.filename, 'r') as fd:
            process_function(fd, **keywords_for_process)


if __name__ == "__main__":
    main()
