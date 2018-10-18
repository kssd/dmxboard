---
name: DMXBoard
version: 1.0.0
description: DMX512 receiver with six channel outputs, a four-digit TM1637 address display, and push-button address configuration. The starting address is stored in EEPROM so it is retained after power-off
---
# DMXBoard

Arduino-based DMX512 receiver with six channel outputs, a four-digit TM1637 address display, and push-button address configuration. The starting address is stored in EEPROM so it is retained after power-off.

## Features

- Receives DMX512 at 250 kbaud, 8N2.
- Maps six consecutive DMX channels to the board outputs.
- Displays the starting address and blinks during configuration.
- Saves the selected address in EEPROM.
- Includes a custom AVR USART-based `DMXDriver` library with master and slave support. The board firmware uses slave mode.

## Hardware

- Arduino Mega 2560 (ATmega2560).
- DMX-compatible RS-485 transceiver.
- TM1637 four-digit display.
- SET, INC, and DEC buttons with external input biasing.
- Output circuitry suitable for the connected load.

The firmware and checked-in PlatformIO configuration target **Arduino Mega 2560 / ATmega2560 at 16 MHz**. The DMX driver uses USART0; SET uses an external interrupt, while INC/DEC are polled during address configuration.

### Pin assignments

| Function | Pin | Notes |
| --- | --- | --- |
| DMX mode | D13 | MASTER = HIGH; SLAVE = LOW |
| SET | D3 | External interrupt |
| INC | D4 | Polled rising edge in CONFIG |
| DEC | D5 | Polled rising edge in CONFIG |
| Display clock | D7 | TM1637 CLK |
| Display data | D6 | TM1637 DIO |
| Channel outputs 1–6 | A0–A5 | Consecutive DMX channels |
| DMX receive/transmit | D0 (RX0)/D1 (TX0) | Hardware USART0 |

D4/D5 have neither pin-change nor external interrupts on Mega. They retain the existing wiring and are polled for rising edges only during CONFIG, with levels sampled on entry so a held button does not count as a new press. Polling can miss pulses shorter than a loop iteration. Button inputs do not enable internal pull-ups.

**Output limitation:** A0–A5 are not hardware PWM pins on Mega. Although the firmware calls `analogWrite()`, these pins do not produce proportional PWM output.

Use an RS-485 transceiver between the DMX bus and the microcontroller. Do not connect DMX bus signals directly to Arduino pins or drive loads directly from the board's logic outputs.

## Build

This project uses PlatformIO and the Arduino framework. Install PlatformIO, then run from the project directory:

```sh
platformio run -e megaatmega2560
```

The display dependency is declared in `platformio.ini`. The custom DMX driver is included under `lib/DMXDriver/`.

To upload to a connected Mega after checking the wiring and selected port:

```sh
platformio run -e megaatmega2560 -t upload
```

Uploading starts the firmware on the board; disconnect loads before testing.

## Set the DMX address

1. Power on the board. It loads the saved address and starts receiving DMX.
2. Press SET to enter address configuration. Reception stops and the display blinks.
3. Use INC and DEC to select the address.
4. Press SET again to save the address and resume reception.

Addresses range from 1 to 512; an out-of-range value resets to 1. Outputs whose channel numbers exceed 512 are set to zero.

**Current button behavior:** INC subtracts one and DEC adds one. The function names do not match the address direction.

## DMXDriver

`DMXDriver` accesses the AVR USART and interrupt vectors directly. Its interface provides initialization, channel reads and writes, maximum-channel selection, update callbacks, and termination.

- Channel numbers are 1-based, from 1 to 512.
- The driver owns the USART; do not use Arduino `Serial` on the same peripheral.
- Receive-update callbacks execute inside the receive ISR with reception temporarily disabled. Keep them short and nonblocking.
- Master transmission is implemented in the library but is not used by the board application.

## Known limitations

- Receive-update handling has an outstanding FIXME and needs validation with complete 512-channel frames.
- Button debouncing is not implemented.
- EEPROM configuration is stored as a raw structure at offset zero. A sequence mismatch on initialization or the reset path clears the entire EEPROM.
- Pin assignments, interrupt timing, DMX reception, and output behavior must be checked on the actual circuit. A successful build alone does not verify the hardware.
