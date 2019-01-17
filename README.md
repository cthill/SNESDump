# SNES Dump
Arduino powered interface for Super Nintendo Entertainment System game carts. A DIY alternative to the [retrode](http://www.retrode.org/).

Features:
* Read cart info from header
* Dump cart ROM
* Backup cart SRAM
* Restore cart SRAM


## Game Compatibility
Games that use certain enhancement chips are not compatible with this setup. Specifically, enhancement chips that sit between the console and the ROM/SRAM will not work. These chips, like the SA1, require a signal from the [Nintendo CIC](https://en.wikipedia.org/wiki/CIC_%28Nintendo%29) lockout chip to start up. No cart data can be accessed without the CIC signal. In the future, I may integrate the CIC chip in the design or emulate it in the firmware.

Most games do not use enhancement chips. For a list of which games use which enhancement chips, see: [List of Super NES games that use enhancement chips](https://en.wikipedia.org/wiki/List_of_Super_NES_enhancement_chips#List_of_Super_NES_games_that_use_enhancement_chips).

Confirmed incompatible:
* SA1
* S-DD1

Confirmed compatible:
* Super FX
* DSP-1


## Desktop Software
The desktop software is written in python and provides an interactive text-based interface for reading and writing to carts. It communicates with the Arduino using the built in USB to serial converter. It has been tested on macOS and Linux.


## Firmware
The firmware is written against the Arduino standard libraries and therefore requires the Arduino IDE and toolchain.

The firmware is very simple. The majority of the logic for dumping the carts is implemented on the desktop side. It communicates with the desktop software over serial. The firmware has three main functions:

1. `CTRL` - for setting cartridge control lines
2. `READSECTION` - for reading chunks of ROM and SRAM
3. `WRITESECTION` - for writing to SRAM


## Hardware
The main hardware components are an Arduino Nano microcontroller (ATmega328P based), three shift registers, and an original SNES cart edge connector.

The basic design is as follows. The A and B buses are connected to three 8 bit shift registers. The B bus occupies the most upper 8 bits. The A bus occupies the lower 16 bits. The shift registers are connected to the Arduino's hardware SPI pins. This allows for much higher data rates than controlling the registers through software. One byte can be shifted out by writing to the `SPDR` register or by calling `SPI.transfer(byte)`. The data bus is connected directly to the microcontroller, and each line is has a 10.0 kΩ pull down resistor.

SNES carts have a 1.2mm card thickness and 2.50mm pin pitch. I do not know of any off the shelf components that will work in the place of an original connector. An original cart connector can be obtained from a broken SNES console. This requires desoldering skills. The cart connector [pinout is documented at the end](#pinout).

Be sure to use an Arduino board with a legitimate FTDI USB to serial converter chip. Some clone chips have issues with higher baud rates. If you are having issues with an Arduino board that does not have an FTDI chip, try lowering the baud rate.

Parts list:
* 1x SNES cartridge connector
* 1x Arduino or AVR compatible micro controller (FTDI USB to serial converter recommended)
* 8x 10.0 kΩ resistors
* 3x 74HC595 shift registers
* Wire for connecting address bus, data bus, and control lines
 * A good source of wire for projects like this is old PATA cables

## Circuit

<img src="/images/circuit.png" width="600"/>

## SNES Carts
SNES cartridges use a 62 pin edge connector and have three buses:

1. `A` - 16 bit address bus
2. `B` - 8 bit bank/page select
3. `Data` - 8 bit bi-directional data bus

Full addresses are notated the the following format: `$BB:AAAA`, where 'BB' is an 8 bit hex value corresponding to the value on bus B, and 'AAAA' is a 16 bit hex value corresponding to the value on bus A. For example: `$00:7FFF`.

Information about the cartridge is found in the cart header. The header is 32 bytes of data located at `$00:FFC0`. When interfacing with a cart, you must first read the header to determine the layout, ROM size, SRAM size, number of banks, etc.

<table>
  <tr>
    <th>Address</th>
    <th>Length</th>
    <th>Data</th>
  </tr>
  <tr>
    <td>$00:FFC0</td>
    <td>21 bytes</td>
    <td>Game Title</td>
  </tr>
  <tr>
    <td>$00:FFD5</td>
    <td>1 byte</td>
    <td>ROM Makeup</td>
  </tr>
  <tr>
    <td>$00:FFD6</td>
    <td>1 byte</td>
    <td>ROM Type</td>
  </tr>
  <tr>
    <td>$00:FFD7</td>
    <td>1 byte</td>
    <td>ROM Size</td>
  </tr>
  <tr>
    <td>$00:FFD8</td>
    <td>1 byte</td>
    <td>SRAM Size</td>
  </tr>
  <tr>
    <td>$00:FFD9</td>
    <td>1 byte</td>
    <td>Country code</td>
  </tr>
  <tr>
    <td>$00:FFDA</td>
    <td>1 byte</td>
    <td>0x33 (fixed value?)</td>
  </tr>
  <tr>
    <td>$00:FFDB</td>
    <td>1 byte</td>
    <td>?</td>
  </tr>
  <tr>
    <td>$00:FFDC</td>
    <td>2 bytes</td>
    <td>Complement check</td>
  </tr>
  <tr>
    <td>$00:FFDE</td>
    <td>2 bytes</td>
    <td>Checksum</td>
  </tr>
</table>

There are two cart layouts: HiROM and LoROM. The lowest bit of the ROM makeup field indicates the layout (0 == LoROM, 1 == HiROM).

LoROM carts have page 32KB pages. A15 is not used and usually internally disconnected. `$0000` to `$7FFF` is a mirror of `$8000` to `$FFFF`. Pull A15 high when reading LoROM carts. HiROM carts have 64KB pages. All 16 lines of bus A are used.

Reading ROM (# of banks can be calculated from data in cart header):
* `LoROM`: Read all banks, from address $8000-$FFFF
* `HiROM`: Read all banks, from address $0000-$FFFF

Reading SRAM (size of SRAM in cart header):
* `LoROM`: Read from $30:8000 on
* `HiROM`: Read from $20:6000 on

Writing SRAM (size of SRAM in cart header):
* `LoROM`: Write from $30:8000 on
* `HiROM`: Write from $20:6000 on

In addition to the three buses, the carts have four control lines (active low):
* `RD` - Read. Pull low when reading data.
* `WR` - Write. Pull low when writing data.
* `CS` - Cart select.
* `RESET` - Reset. Pull high when accessing cart.

<table>
  <tr>
    <th></th>
    <th colspan="2">Read ROM</th>
    <th colspan="2">Read SRAM</th>
    <th colspan="2">Write SRAM</th>
  </tr>
  <tr>
    <td></td><td>HiROM</td><td>LoROM</td><td>HiROM</td><td>LoROM</td><td>HiROM</td><td>LoROM</td>
  </tr>
  <tr>
    <td>RD</td><td>0</td><td>0</td><td>1</td><td>1</td><td>1</td><td>1</td>
  </tr>
  <tr>
    <td>WR</td><td>1</td><td>1</td><td>0</td><td>0</td><td>0</td><td>0</td>
  </tr>
  <tr>
    <td>CS</td><td>0</td><td>0</td><td>1</td><td>0</td><td>1</td><td>0</td>
  </tr>
  <tr>
    <td>RESET</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td>
  </tr>
</table>

## Cart Pinout
Bus A is marked in light blue. Bus B is marked in dark blue. The data bus is marked in green. The control lines are orange. Power pins are marked in yellow. The gray pins are not used for this project.

<img src="/images/pinout.png" width="350"/>
