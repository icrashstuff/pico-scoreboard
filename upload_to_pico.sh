#!/bin/sh
# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2025 Ian Hangartner <icrashstuff at outlook dot com>
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
# DEALINGS IN THE SOFTWARE.

set -e

SCRIPT_NAME="$0"

print_usage()
{
echo "Usage: ${SCRIPT_NAME} uf2_binary pico_serial_number [fallback_usb_manufacturer fallback_usb_product]"
}

# Check that binary file exists
if [ ! -f "$1" ]
then
    echo "'$1' does not exist!"
    print_usage
    exit 1
fi

SERIAL="$2"

# Check that something was passed for the serial number field
if [ "$SERIAL" = "" ]
then
    echo "No serial number provided"
    print_usage
    exit 1
fi

if [ $# -ge 4 ]
then
    SERIAL_NUMBER_GOOD=0
    FALLBACK_SERIAL=""
    echo "Fallback USB manufacturer: $3"
    echo "Fallback USB product:      $4"

    for devpath in $(find /sys/bus/usb/devices/ -type l)
    do
        if [ -f "$devpath/product" ] && [ -f "$devpath/manufacturer" ] && [ -f "$devpath/serial" ] && [ "$SERIAL_NUMBER_GOOD" = "0" ]
        then
            [ "`cat "$devpath/serial"`" = $SERIAL ] && SERIAL_NUMBER_GOOD=1
            [ "`cat "$devpath/product"`" = "$4" ] && [ "`cat "$devpath/manufacturer"`" = "$3" ] && FALLBACK_SERIAL=`cat "$devpath/serial"`
        fi
    done

    echo "Fallback USB serial:       $FALLBACK_SERIAL"
    echo "Primary Serial Good:       $SERIAL_NUMBER_GOOD"

    [ "$SERIAL_NUMBER_GOOD" = "0" ] && [ "$FALLBACK_SERIAL" != "" ] && SERIAL=$FALLBACK_SERIAL
fi

# Reset via magic baud rate (if stty is present and a suitable serial path is found)
PICO_STDIO_USB_RESET_MAGIC_BAUD_RATE=1200
if type stty
then
    SER_PATH=$(find /dev/serial/by-id/ -name "*${SERIAL}*" | head -n1)
    if [ "$SER_PATH" != "" ]
    then
        echo "Attempting to force reset via magic baud rate"
        stty -F "$SER_PATH" speed ${PICO_STDIO_USB_RESET_MAGIC_BAUD_RATE}
        sleep 1
    fi
fi

picotool load  --ser "$SERIAL" -f "$1"
picotool reboot --ser "$SERIAL" || true # Picotool doesn't reboot if pico is already in bootsel mode
