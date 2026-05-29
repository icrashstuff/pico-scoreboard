# Scoreboard controller protocol

## Controller info

Nevco 2-MC scoreboard controller with the following controls
- Home score (Range 0-199)
  - 100's place (Limited to a value of 'OFF' or '1')
  - 10's place
  - 1's place
  - Bonus
- Guest score (Range 0-199)
  - 100's place (Limited to a value of 'OFF' or '1')
  - 10's place
  - 1's place
  - Bonus
- Timer:
  - Time up/Time down
  - Reset to zero
  - 10's place (Limited to 'OFF','1','2')
  - 1's place
  - Reset
  - Time in/out
- Horn
 - Press to sound

---

# Transmission layer

A bitstream is transmitted as a signal with the following states:

| Char |  Name   |                    Description                    |
|------|---------|---------------------------------------------------|
| `r`  | Restart | Logic high for 4us                                |
| `1`  | Bit 1   | Logic high for 3us, followed by logic low for 1us |
| `0`  | Bit 0   | Logic high for 1us, followed by logic low for 3us |

2-MC controller seems to have a tolerance of ~50ns for all timing values

### Stats

- Frequency: 1MHz
- Bitrate: 250kbit/s

### Physical

- Voltage at line driver: 5v - 5.5v
- Current draw: Unsure, but probably 10-20 mA as the 2-MC uses chips from the original 7400 TTL series so the scoreboard likely does as well

---

# Bit layer

Bitstream seems to be organized as a series of 16 bit wide "commands". Commands need not be in any particular order and may be repeated between restarts.

## Command format

|  Bits |            Description             |
|-------|------------------------------------|
| 0-3   | Unknown (Always '1111' or 'r111') |
| 4-7   | Write Address                      |
| 8-11  | Data                               |
| 12-15 | Unknown (Always '1111')            |

## Addresses

Addresses seem to correspond with the divisions in the MUX input section of the 2-MC circuit board

|  Address   |     Bit 0 Description     |     Bit 1 Description     |     Bit 2 Description     |     Bit 3 Description     |
|------------|---------------------------|---------------------------|---------------------------|---------------------------|
| 0000 (0x0) | Guest Score 10's place    | Guest Score 10's place    | Guest Score 10's place    | Guest Score 10's place    |
| 1000 (0x1) | Guest Score 1's place     | Guest Score 1's place     | Guest Score 1's place     | Guest Score 1's place     |
| 0100 (0x2) | Unknown (Always 1)        | Unknown (Always 1)        | Home Score 100's place    | Guest Score 100's place   |
| 1100 (0x3) | Period 1                  | Period 2                  | Period 3                  | Period 4                  |
| 0010 (0x4) | Unknown (Always 1)        | Unknown (Always 1)        | Unknown (Always 1)        | Unknown (Always 1)        |
| 1110 (0x5) | Timer digit 0             | Timer digit 0             | Timer digit 0             | Timer digit 0             |
| 0110 (0x6) | Timer digit 1             | Timer digit 1             | Timer digit 1             | **Horn**                  |
| 1010 (0x7) | Timer digit 2             | Timer digit 2             | Timer digit 2             | Timer digit 2             |
| 0001 (0x8) | Timer digit 3 Display '1' | Timer digit 3 Display '2' | Home Bonus                | Guest Bonus               |
| 1001 (0x9) | Home Score 10's place     | Home Score 10's place     | Home Score 10's place     | Home Score 10's place     |
| 0101 (0xA) | Home Score 1's place      | Home Score 1's place      | Home Score 1's place      | Home Score 1's place      |
| 1101 (0xB) | Unknown (Always 1)        | Unknown (Always 1)        | Unknown (Always 1)        | Unknown (Always 1)        |
| 0011 (0xC) | Unknown (Always 1)        | Unknown (Always 1)        | Unknown (Always 1)        | Unknown (Always 1)        |
| 1011 (0xD) | Unknown (Always 1)        | Unknown (Always 1)        | Unknown (Always 1)        | Unknown (Always 1)        |
| 0111 (0xE) | Unknown (Always 1)        | Unknown (Always 1)        | Unknown (Always 1)        | Unknown (Always 1)        |
| 1111 (0xF) | Unknown (Always 1)        | Unknown (Always 1)        | Unknown (Always 1)        | Unknown (Always 1)        |

Some addresses have side effects when written to **(TABLE INCOMPLETE)**

|  Address   |         Side effects         |
|------------|------------------------------|
| 0110 (0x6) | Illuminates home Lamp, Guests Lamp |
| 1010 (0x7) | Illuminates timer separator lamps, Timer digits 1 & 2 top right corner lamp, middle right joint lamp, and bottom right corner lamps |
