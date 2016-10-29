#include<SPI.h>

const int AddressLatchPin = 10;

const int snesReadPin = A1; //Cart pin 23 - aka /RD - Address bus read
const int snesWritePin = A2; //Cart pin 54 - aka /WR - Address bus write
const int snesCartPin = A3; //Cart pin 49 - aka /CS, /ROMSEL, /Cart - Goes low when reading ROM
const int snesResetPin = A4; //Cart pin 26 - SNES reset pin. Goes high when reading cart

typedef enum COMMANDS {
  CTRL,
  READSECTION,
  WRITESECTION
};

void setup() {
  //begin serial
  Serial.begin(2000000);
  //erial.begin();
  UBRR0 = 0; //max baud rate
  bitSet(UCSR0A, U2X0); // change UART divider from 16 to 8  for double transmission speed

  //begin spi
  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV2); //8MHz

  setDataBusDir(INPUT);

  //setup cart control pins
  pinMode(snesCartPin, OUTPUT);
  pinMode(snesReadPin, OUTPUT);
  pinMode(snesWritePin, OUTPUT);
  pinMode(snesResetPin, OUTPUT);

  //read the rom header
  //readHeader();

  //Write byte to indicate device ready
  Serial.write(0x00);
}

void loop() {
  int incomingByte = serialReadBlocking();
  switch(incomingByte) {
    // set the cartridge control lines
    case CTRL:
    {
      setCtrlLines(serialReadBlocking());
    }
    break;

    // read a section of data from the cart
    case READSECTION:
    {
      byte bank = serialReadBlocking();
      byte startAddrHi = serialReadBlocking();
      byte startAddrLow = serialReadBlocking();
      byte endAddrHi = serialReadBlocking();
      byte endAddrLow = serialReadBlocking();

      unsigned int addr = bytesToInt(startAddrHi, startAddrLow);
      unsigned int endAddr = bytesToInt(endAddrHi, endAddrLow);
      setDataBusDir(INPUT);

      while (true) {
        writeAddrBus(bank, addr);
        Serial.write(readDataBus());

        /* We must break out of the loop this way because of the potential case
         * where addresses 0x0000 - 0xffff (inclusive) must be used. A standard
         * loop won't work because it will overflow the 16 bit unsigned int.
         */
        if (addr == endAddr) {
          break;
        }
        addr++;
      }
      Serial.flush();
    }
    break;

    case WRITESECTION:
    {
      byte bank = serialReadBlocking();
      byte startAddrHi = serialReadBlocking();
      byte startAddrLow = serialReadBlocking();
      byte endAddrHi = serialReadBlocking();
      byte endAddrLow = serialReadBlocking();

      unsigned int addr = bytesToInt(startAddrHi, startAddrLow);
      unsigned int endAddr = bytesToInt(endAddrHi, endAddrLow);
      setDataBusDir(OUTPUT);

      while (true) {
        writeAddrBus(bank, addr);
        writeDataBus(serialReadBlocking());

        /* We must break out of the loop this way because of the potential case
         * where addresses 0x0000 - 0xffff (inclusive) must be used. A standard
         * loop won't work because it will overflow the 16 bit unsigned int.
         */
        if (addr == endAddr) {
          break;
        }
        addr++;
      }
    }
    break;
  }
}

byte serialReadBlocking() {
  while(Serial.available() == 0);
  return Serial.read();
}

unsigned int bytesToInt(byte hi, byte low) {
  return ((unsigned int) hi << 8) | low;
}

void setCtrlLines(byte s) {
  digitalWrite(snesReadPin, s & 0x8);
  digitalWrite(snesWritePin, s & 0x4);
  digitalWrite(snesCartPin, s & 0x2);
  digitalWrite(snesResetPin, s & 0x1);
}

/* Write a value out to the address bus
 * Uses direct port manipulation and
 * hardware spi for better performance
 */
void writeAddrBus(byte bank, unsigned int addr) {
  PORTB &= ~(B100); //Set AddressLatchPin low
  SPI.transfer(bank); // shift out bank
  SPI.transfer(addr >> 8); // shift out address upper byte
  SPI.transfer(addr); // shift out address lower byte
  PORTB |= (B100); //Set AddressLatchPin high
}

//Read byte from data bus
byte readDataBus(){
  //Digital pins 2-7 (PIND) are connected to snes data lines 2-7.
  //Digital pins 8 and 9 (PINB) are connected to data lines 0 and 1 respectively
  //This line of code takes the data from pins 2-7 and from 8&9 and combines them into one byte.
  //The resulting bye looks like this: (pin7 pin6 pin5 pin4 pin3 pin2 pin9 pin8)
  return (PIND & ~0x03) | (PINB & 0x03);
}

//Write byte to data bus
void writeDataBus(byte data) {
  digitalWrite(8, bitRead(data, 0));
  digitalWrite(9, bitRead(data, 1));
  for (int i = 2; i < 8; i++) {
    digitalWrite(i, bitRead(data, i));
  }
}

//Set the data bus to output or input
// false => INPUT, true => OUTPUT
void setDataBusDir(bool dir) {
  for (int p = 2; p <=9; p++){
    pinMode(p, dir);
  }
}
