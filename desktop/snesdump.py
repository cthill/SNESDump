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
baud = 1000000

def main():
    print "SNESDump v1"

    #get enviroment specific list of serial ports
    if sys.platform.startswith("win"):
        ports = ["COM" + str(i + 1) for i in range(256)]
    elif sys.platform.startswith("linux") or sys.platform.startswith("cygwin"):
        ports = glob.glob("/dev/tty[A-Za-z]*")
    elif sys.platform.startswith("darwin"):
        ports = glob.glob("/dev/tty.*")
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

    # wait for device to signal it is ready
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
            #send the info command to the arduino
            port.write(struct.pack("B", 0));
            header = port.read(64)
            print "", header[:21]
            print "", (1 << ord(header[23])), "KB ROM"
            print "", (1 << ord(header[24])), "KB SRAM"
            print "", "LoROM" if (ord(header[22]) & 1) == 0 else "HiROM"

        elif action == "d":
            file_name = raw_input("Please enter an output filename: ")
            output_file = open(file_name, "wb")

            port.write(struct.pack("B", 1)); #send the dump rom command to the arduino
            rom_size = (1 << ord(port.read())) * 1024 #(2 to the power of port.read()) * 1024
            bytes_read = 0;

            def printDR():
                s = sched.scheduler(time.time, time.sleep)
                def doPrint(sc, last_bytes_read):
                    diff = bytes_read - last_bytes_read
                    last_bytes_read = bytes_read
                    sys.stdout.write("\r Dumping ROM {0:,}/{1:,} bytes  {2:,} bytes/sec".format(bytes_read, rom_size, diff))
                    sys.stdout.flush()
                    sc.enter(1, 1, doPrint, (sc, last_bytes_read))
                # do your stuff
                s.enter(0, 1, doPrint, (s, 0))
                s.run()

            #thread.start_new_thread(printDR, ())

            while rom_size > bytes_read:
                bytes_waiting = port.inWaiting()
                output_file.write(port.read(bytes_waiting))
                bytes_read += bytes_waiting
                sys.stdout.write("\r Dumping ROM {0:,}/{1:,} bytes".format(bytes_read, rom_size))
                sys.stdout.flush()

            output_file.close()
            print "\n Done."
        elif action == "s":
            file_name = raw_input("Please enter an output filename: ")
            output_file = open(file_name, "wb")

            port.write(struct.pack("B", 2)); #send the dump sram command to the arduino
            sram_size = (1 << ord(port.read())) * 1024 #(2 to the power of port.read()) * 1024
            bytes_read = 0;

            while sram_size > bytes_read:
                output_file.write(port.read())
                bytes_read += 1
                sys.stdout.write("\r Dumping SRAM {0}/{1} bytes".format(bytes_read, sram_size))
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
                print "No such file. ",
                input_file = get_input_file()

            file_size = os.fstat(input_file.fileno()).st_size

            port.write(struct.pack("B", 3)); #send the write sram command to the arduino
            sram_size = (1 << ord(port.read())) * 1024 #(2 to the power of port.read()) * 1024

            if file_size != sram_size:
                print "File size does not match cartridge SRAM size."
            else:
                bytes_written = 0;
                while input_file.tell() < file_size:
                    this_byte = ord(input_file.read(1))
                    port.write(struct.pack("B", this_byte))
                    bytes_written += 1
                    time.sleep(0.001) #add a small delay
                    sys.stdout.write("\r Writing SRAM {0}/{1} bytes".format(bytes_written, sram_size))
                    sys.stdout.flush()
                print "\n Done."
            input_file.close()
        elif action == "h":
            print_options()
        elif action == "q":
            quit = True
        else:
            print "Invalid selection. Type \"h\" for help",

    port.close()

#code to handle Ctrl-c
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
