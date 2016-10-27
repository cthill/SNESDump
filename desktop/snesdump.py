import os
import sys
import glob
import sched
import serial
import signal
import struct
import time
import thread

port = None
baud = 2000000

commands = {
    'CTRL': chr(0),
    'READSECTION': chr(1),
    'WRITESECTION': chr(2)
}

countries = [
    'Japan (NTSC)','USA (NTSC)','Europe, Oceania, Asia (PAL)','Sweden (PAL)',
    'Finland (PAL)','Denmark (PAL)','France (PAL)','Holland (PAL)',
    'Spain (PAL)','Germany, Austria, Switz (PAL)','Italy (PAL)',
    'Hong Kong, China (PAL)','Indonesia (PAL)','Korea (PAL)'
]

def main():
    #get enviroment specific list of serial ports
    if sys.platform.startswith("win"):
        ports = ["COM" + str(i + 1) for i in range(256)]
    elif sys.platform.startswith("linux") or sys.platform.startswith("cygwin"):
        ports = glob.glob("/dev/ttyUSB*")
    elif sys.platform.startswith("darwin"):
        ports = glob.glob("/dev/tty*")
    else:
        raise EnvironmentError("Unsupported platform")

    #print the list of open ports to the screen
    num_ports = 0
    available_ports = []
    for port in ports:
        available_ports.append(port)
        print " " + str(num_ports) + " - " + port
        num_ports += 1

    #ask the user to select a serial port
    def get_port():
        try:
            return int(raw_input("Please select a device: "))
        except (ValueError):
            return -1
            pass

    port_selection = get_port()
    while port_selection < 0 or port_selection >= num_ports:
        print "Invalid selection.",
        port_selection = get_port()

    #open the serial port
    try:
        port = serial.Serial(available_ports[port_selection], baud)
    except (OSError, serial.SerialException):
        raise OSError("Could not open serial port")

    # wait for device to signal it is ready.
    port.read(1)

    #print the options to the screen
    def print_options():
        print " i - Cart Info\n d - Dump ROM\n s - Dump SRAM\n w - Write SRAM\n h - Show this screen\n q - Quit"
    print_options()

    #the main loop
    quit = False
    while not quit:
        action = raw_input("Please select an action: ").lower()
        if action == "i":
            print_cart_info(get_header(port))

        elif action == "d":
            file_name = raw_input("Please enter an output filename: ")
            output_file = open(file_name, "wb")

            header = get_header(port)
            hirom = (header[21] & 1)
            # lorom carts are read from 0x8000 to 0xffff
            read_offset = 0x0 if hirom else 0x8000
            # lorom banks are 0x8000 in size
            bank_size = 0x10000 - read_offset
            # compute size of entire rom
            rom_size = (1 << header[23]) * 1024
            # compute number of banks
            num_banks = rom_size/bank_size
            print_cart_info(header)
            set_ctrl_lines(port, False, True, False, True)
            total_bytes_read = 0
            # start dumping, 1 bank at a time
            for bank in range(0, num_banks):
                # send read section command
                port.write(commands['READSECTION'])
                # write bank to read from
                port.write(chr(bank));
                # write startAddr
                write_addr(port, read_offset)
                # write endAddr
                write_addr(port, read_offset + bank_size - 1)

                bytes_read = 0;
                # read bank data in loop
                while bytes_read < bank_size:
                    num_to_read = port.inWaiting()
                    output_file.write(port.read(num_to_read))
                    bytes_read += num_to_read
                    total_bytes_read += num_to_read
                    sys.stdout.write("\r Dumping ROM {0:,}/{1:,} bytes".format(total_bytes_read, rom_size))
                    sys.stdout.flush()

            output_file.close()
            print "\n Done."

        elif action == "s":
            file_name = raw_input("Please enter an output filename: ")
            output_file = open(file_name, "wb")

            header = get_header(port)
            hirom = (header[21] & 1)
            read_offset = 0x0 if hirom else 0x8000
            sram_size = (1 << header[24]) * 1024

            print_cart_info(header)

            # cart select is high for hirom carts, low for lorom carts when doing sram reads
            set_ctrl_lines(port, False, True, hirom, True)

            # compute bank and addresses to read from
            bank = 0b00100000
            start_addr = read_offset
            if hirom:
                start_addr |= 0b0110000000000000
            else:
                start_addr |= 0b01010000
            end_addr = start_addr + sram_size - 1

            # send read section command
            port.write(commands['READSECTION'])
            # write bank number
            port.write(chr(bank))
            # write startAdd
            write_addr(port, start_addr)
            # write endAddr
            write_addr(port, end_addr)

            bytes_read = 0;
            # read bank data in loop
            while bytes_read < sram_size:
                num_to_read = port.inWaiting()
                output_file.write(port.read(num_to_read))
                bytes_read += num_to_read
                sys.stdout.write("\r Dumping SRAM {0:,}/{1:,} bytes".format(bytes_read, sram_size))
                sys.stdout.flush()

            output_file.close()
            print "\n Done."
        elif action == "w":
            def get_input_file():
                try:
                    file_name = raw_input("Please enter an input filename: ")
                    return open(file_name, "rb")
                except IOError:
                    return None

            input_file = get_input_file()
            while not input_file:
                print "No such file."
                continue

            file_size = os.fstat(input_file.fileno()).st_size

            header = get_header(port)
            hirom = (header[21] & 1)
            read_offset = 0x0 if hirom else 0x8000
            sram_size = (1 << header[24]) * 1024

            print_cart_info(header)

            if sram_size != file_size:
                print "File size mismatch! File: {}, SRAM: {}".format(file_size, sram_size)
                input_file.close()
                continue

            # cart select is high for hirom carts, low for lorom carts when doing sram writes
            set_ctrl_lines(port, True, False, hirom, True)

            # compute bank and addresses to write to
            bank = 0b00100000
            start_addr = read_offset
            if hirom:
                start_addr |= 0b0110000000000000
            else:
                start_addr |= 0b01010000
            end_addr = start_addr + sram_size - 1

            # send read section command
            port.write(commands['WRITESECTION'])
            # write bank number
            port.write(chr(bank))
            # write startAdd
            write_addr(port, start_addr)
            # write endAddr
            write_addr(port, end_addr)

            bytes_written = 0;
            while input_file.tell() < file_size:
                this_byte = input_file.read(1)
                port.write(this_byte)
                bytes_written += 1
                time.sleep(0.001) #add a small delay
                sys.stdout.write("\r Writing SRAM {0}/{1} bytes".format(bytes_written, sram_size))
                sys.stdout.flush()

            input_file.close()
            print "\n Done."
        elif action == "h":
            print_options()
        elif action == "q":
            quit = True
        else:
            print "Invalid selection.",

    port.close()

# read cart header in bank 0, 0xffc0 to 0xffde
def get_header(port):
    # write control line states
    set_ctrl_lines(port, False, True, False, True)
    # send read section command
    port.write(commands['READSECTION'])
    # write bank number
    port.write(chr(0))
    # write startAdd
    write_addr(port, 0xffc0)
    # write endAddr
    write_addr(port, 0xffdf)
    # read 32 byte header
    return bytearray(port.read(32))

def print_cart_info(header):
    title = str(header[:21]).strip()
    layout =  "HiROM" if (header[21] & 1) else "LoROM"
    rom_size = (1 << header[23])
    sram_size = (1 << header[24])
    country_code = header[25]
    country = countries[country_code] if country_code < len(countries) else str(country_code)
    version = header[27]
    checksum = (header[30] << 8) | header[31]
    print " {}, {}, {} KB ROM, {} KB SRAM\n Country: {}, Version: {}, Checksum: 0x{:02X}".format(title, layout, rom_size, sram_size, country, version, checksum)

# write a 16 bit address to the serial port
def write_addr(port, addr):
    port.write(chr(addr >> 8 & 0xff))
    port.write(chr(addr & 0xff))

# set control line states (lines are active low)
# 4 bits of information (most to least sig): read, write, cart select, reset
def set_ctrl_lines(port, read, write, cart, reset):
    value = (read << 3) | (write << 2) | (cart << 1) | (reset)
    port.write(commands['CTRL'])
    port.write(chr(value))

#code to handle SIGINT
def sigint_handler(signum, frame):
    signal.signal(signal.SIGINT, sigint)
    if port is not None:
        port.close()
    sys.exit(1)
    signal.signal(signal.SIGINT, sigint_handler)

if __name__ == '__main__':
    sigint = signal.getsignal(signal.SIGINT)
    signal.signal(signal.SIGINT, sigint_handler)
    main()
