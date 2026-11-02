# Arduino Nano variant

This is an alternative build. The main clock in this repo has no microcontroller and no code, and that's still the recommended way to build it. Use this variant if you'd rather have buttons for setting the time, or want the time kept across power cuts with an RTC module.

The Nano replaces everything on board 2: the CD4060 timebase, the four CD4518 counters, the CD4081 reset gates, the crystal and its parts. Board 1 (the four CD4511 decoders and the displays) is wired almost exactly the same as the main build.

## How it works

All four CD4511 decoders share one 4-bit data bus from the Nano. To change a digit, the Nano puts the number on the bus and briefly pulls that decoder's LE (latch enable) pin low, then high again. The decoder latches the digit and keeps showing it while the Nano moves on to the next one. The displays never flicker because nothing is multiplexed.

Timekeeping:

- **With a DS3231 module** on A4/A5, the Nano reads the time from it. The DS3231 has its own temperature-compensated crystal and a coin cell, so it keeps accurate time (around a minute a year) and survives power cuts.
- **Without one**, the sketch counts on the Nano's own clock. Most Nanos use a ceramic resonator that can be off by several minutes a day, and the time resets to 00:00 whenever power is lost. You can trim the drift with MS_PER_SECOND at the top of the sketch.

The sketch detects the DS3231 by itself at startup, so the same code works either way.

## Parts

Keep from the main build: U6–U9 (CD4511), DS1–DS4, R1–R29, C9–C12, both breadboards, the MB102 module, jumper wires and the wall adapter.

Not needed: U1–U5, U10, Y1, C1–C2, C3–C8, R30–R34.

Add (not in bom.csv):

| Part | Qty | Notes |
|---|---|---|
| Arduino Nano (ATmega328P) or clone | 1 | Clones with a CH340 USB chip may need a driver on Windows |
| 6×6 mm tactile push button | 2 | For setting hours and minutes |
| DS3231 RTC module | 1 | Optional, but strongly recommended |

## Wiring

Put the Nano on board 2 across the center channel.

### Power

| Nano pin | Connect to |
|---|---|
| 5V | +5V rail |
| GND | GND rail |

Unplug the wall adapter while the Nano is connected to your computer over USB for uploading.

### Decoders (board 1)

Wire each CD4511 as in step 7 of the main README, with two changes:

1. **Inputs A, B, C, D** no longer go to counter pins. Instead, connect pin 7 (A) of all four decoders together, and the same for pin 1 (B), pin 2 (C) and pin 6 (D). That's the shared bus.
2. **LE (pin 5)** no longer goes to GND. Each decoder gets its own Nano pin.

| Nano pin | Connect to |
|---|---|
| D2 | Bus A: pin 7 of U6, U7, U8, U9 |
| D3 | Bus B: pin 1 of U6, U7, U8, U9 |
| D4 | Bus C: pin 2 of U6, U7, U8, U9 |
| D5 | Bus D: pin 6 of U6, U7, U8, U9 |
| D6 | U6 pin 5 (hours tens) |
| D7 | U7 pin 5 (hours ones) |
| D8 | U8 pin 5 (minutes tens) |
| D9 | U9 pin 5 (minutes ones) |
| D10 | R29 (1k) to DS2 pin 5 (blinking decimal point) |

Pins 3 (LT) and 4 (BI) stay at +5V, and the segment wiring and resistors are unchanged.

### Buttons

| Nano pin | Connect to |
|---|---|
| D11 | Hours button, other side to GND |
| D12 | Minutes button, other side to GND |

No resistors needed. The sketch turns on the Nano's internal pull-ups.

### DS3231 (optional)

| Module pin | Connect to |
|---|---|
| VCC | +5V |
| GND | GND |
| SDA | Nano A4 |
| SCL | Nano A5 |

Leave SQW and 32K unconnected.

## Uploading

1. Install the [Arduino IDE](https://www.arduino.cc/en/software).
2. Open logic_clock_nano/logic_clock_nano.ino. No extra libraries are needed.
3. Select **Tools → Board → Arduino Nano**. If the upload fails, try **Tools → Processor → ATmega328P (Old Bootloader)**, which most clones need.
4. Select the port and click Upload.

## Setting the time

- Press the hours button to add one hour. Hold it to keep advancing.
- Press the minutes button to add one minute. It also zeroes the seconds, so you can press it the moment a reference clock ticks over to sync it.

With a DS3231 fitted, the new time is saved to the module straight away.

## Troubleshooting

| Symptom | Check |
|---|---|
| All digits show the same number | An LE pin is still tied to GND, or two LE wires are swapped onto the same Nano pin |
| A digit shows the wrong number | Bus wires out of order: D2 must go to A (pin 7), not pin 1 |
| Time resets to 00:00 after power loss | No DS3231 found. Check SDA/SCL on A4/A5 and the coin cell |
| Clock drifts by minutes a day | Running without a DS3231. Add one, or adjust MS_PER_SECOND |
| Upload fails with "not in sync" | Choose the Old Bootloader processor option |
