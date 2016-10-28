# SNES Dump
Arduino powered interface for Super Nintendo Entertainment System game carts. A poor man's alternative to the [retrode](http://www.retrode.org/).

[Desktop Software](#desktop)
-----
The desktop software is written in python and provides an interactive text-based interface for reading and writing to carts. It has been tested on macOS and Linux.

The software allows users to backup SNES cart ROM data for archival or emulation purposes. It also allows users to transfer save game data between real SNES hardware and emulation software.

Features:
* Read cartridge header
* Dump cart ROM
* Dump cart SRAM
* Write to cart SRAM


[Firmware](#firmware)
----
The firmware is written against the Arduino standard libraries and therefore requires the Arduino IDE and toolchain.

The firmware is very simple. The majority of the logic for dumping the carts is implemented on the desktop side. It communicates with the desktop software over serial. The firmware has three main functions:

1. `CTRL` - for setting cartridge control lines (see: [Carts](#carts))
2. `READSECTION` - for reading chunks of ROM and SRAM
3. `WRITESECTION` - for writing to SRAM


[Hardware](#hardware)
----
BOM:
* 1x SNES cartridge slot
* 1x Arduino or AVR compatible micro controller
 * Can be desoldered from a broken console
* 8x 10.0 kΩ resistors
* 3x 74HC595 shift registers
* Wire for connecting address bus, data bus, and control lines
 * A good source of wire for projects like this is old PATA cables

![Circuit diagram](/images/circuit.png)

[SNES Carts](#carts)
----
SNES cartridges are simple to interface with. They use a 62 pin connector and have three buses:

1. `A` - 16 bit address bus
2. `B` - 8 bit bank/page select
3. `Data` - 8 bit data bus

Addresses are written the the following format: `$BB:AAAA`, where 'BB' is an 8 bit hex value corresponding to bus B, and 'AAAA' is a 16 bit hex value corresponding to bus A. For example: `$00:7FFF`

There are two cart layouts: HiROM and LoROM. LoROM carts have 32KB pages (A15 is not used, pull high). HiROM carts have 64KB pages.

Information about the cartridge (layout, SRAM size, number of banks, title, country code, checksum, etc) is found in the cart header. The header is 32 bytes of data located at `$00:FFC0`. The lowest bit of the ROM makeup field indicates the layout (0 == LoROM, 1 == HiROM).

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

The carts have four control lines (active low):
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

Reading ROM (# of banks can be calculated from data in cart header):
* `LoROM`: Read all banks, from address $8000-$FFFF
* `HiROM`: Read all banks, from address $0000-$FFFF

Reading SRAM (size of SRAM in cart header):
* LoROM: Read from $30:8000 on
* HiROM: Read from $20:6000 on

Writing SRAM (size of SRAM in cart header):
* LoROM: Write from $30:8000 on
* HiROM: Write from $20:6000 on


![Cart pinout](/images/pinout.png)
