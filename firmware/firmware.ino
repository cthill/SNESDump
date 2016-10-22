#include<SPI.h>

const int AddressLatchPin = 10;

const int snesCartPin = A4; //Cart pin 49 - aka /CS, /ROMSEL, /Cart - Goes low when reading ROM
const int snesReadPin = A3; //Cart pin 23 - aka /RD - Address bus A read strobe.
const int snesWritePin = A2; //Cart pin 54 - aka /WR - Address bus A write strobe.
const int snesResetPin = A1; //Cart pin 26 - SNES reset pin. Goes LOW on reset. High when reading cart

const int LoROM = 0;
const int HiROM = 1;

const long bankSize = 0x10000; //65536 bytes

typedef enum Flags { 
  HEADRead,
  ROMRead,
  RAMRead,
  RAMWrite,
  CHUNKRead,
};

byte mode;
boolean done = false;

//vars to store rom header data
int romLayout;
byte cartType;
unsigned long readOffset;
unsigned long romSize;
unsigned long ramSize;
byte romSizeCode;
byte ramSizeCode;

void setup() {
  //begin serial
  Serial.begin(2000000);
  //erial.begin();
  UBRR0 = 0; //max baud rate
  bitSet(UCSR0A, U2X0); // change UART divider from 16 to 8  for double transmission speed

  //begin spi
  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV2); //8MHz
  
  setDataBusDirection(INPUT);

  //setup cart control pins
  pinMode(snesCartPin, OUTPUT);
  pinMode(snesReadPin, OUTPUT);
  pinMode(snesWritePin, OUTPUT);
  pinMode(snesResetPin, OUTPUT);

  //read the rom header
  readHeader();

  //Write byte to indicate device ready
  Serial.write(0x00); 
}

void loop() {
  done = true;
  if (Serial.available() > 0) {
    int incomingByte = Serial.read();
    switch(incomingByte) {
      case HEADRead:
      {
        Serial.write(getHeader(), 64);
        Serial.flush();
      } 
      break;
      
      case ROMRead:
      {
        //Read the header
        readHeader();

        //Write the rom size
        Serial.write(romSizeCode);

        //prepare to dump the cart
        digitalWrite(snesReadPin, LOW);
        digitalWrite(snesCartPin, LOW);
        digitalWrite(snesResetPin, HIGH);
        digitalWrite(snesWritePin, HIGH);
        setDataBusDirection(INPUT);

        //dump the cart
        byte banks = romSize / (bankSize - readOffset);
        dumpCart(banks);

        Serial.flush();
      }
      break;
      
      case RAMRead:
      {
        //Read the header
        readHeader();

        //Write the ram size code
        Serial.write(ramSizeCode);

        //prepare to dump the save data
        digitalWrite(snesReadPin, LOW);
        digitalWrite(snesCartPin, (romLayout == HiROM ? HIGH : LOW));
        digitalWrite(snesResetPin, HIGH);
        digitalWrite(snesWritePin, HIGH);
        setDataBusDirection(INPUT);

        //dump the data
        for (long a = readOffset; a < (readOffset + ramSize); a++) {
          byte bank = 0x00;
          int addr = a;
          
          //LoROM
          if (romLayout == LoROM) {
            bitWrite(bank, 4, 1);
            bitWrite(bank, 6, 1);
          } else if (romLayout == HiROM) {
          //HiROM
            bitWrite(addr, 13, 1);
            bitWrite(addr, 14, 1);
            bitWrite(addr, 15, 0);
            bitWrite(bank, 6, 0);
          }
          //Both
          bitWrite(bank, 5, 1);
          
          address(bank, addr);
          Serial.write(cartRead());
        }
        Serial.flush();
      }
      break;
        
      case RAMWrite:
      {
        //read cart header
        readHeader();
        //write ram size code
        Serial.write(ramSizeCode);
        //prepare to write save data
        digitalWrite(snesReadPin, HIGH);
        digitalWrite(snesCartPin, (romLayout == HiROM ? HIGH : LOW));
        digitalWrite(snesResetPin, HIGH);
        digitalWrite(snesWritePin, LOW);
        setDataBusDirection(OUTPUT);

        //write save data
        int received = 0;
        while (received < ramSize) {
          while (Serial.available()) {
            byte incomingByte = Serial.read();
            byte bank = 0x00;
            unsigned int addr = 0x00;
            
            //LoROM
            if (romLayout == LoROM) {
              bitWrite(bank, 4, 1);
              bitWrite(bank, 6, 1);
            } else if (romLayout == HiROM) {
            //HiROM
              bitWrite(addr, 13, 1);
              bitWrite(addr, 14, 1);
              bitWrite(addr, 15, 0);
              bitWrite(bank, 6, 0);
            }
            //Both
            bitWrite(bank, 5, 1);
            
            address(bank, addr + received);
            cartWrite(incomingByte);
            received++;
          }
        }
      }
      break;

      case CHUNKRead:
      {
        byte bank = serialReadBlocking();
        byte addr_upper = serialReadBlocking();
        byte addr_lower = serialReadBlocking();

        if (bank == 0 && addr_upper == 0xFF && addr_lower == 0xC0) {
          digitalWrite(13, HIGH);
          delay(1000);
          digitalWrite(13, LOW);
          delay(1000);
          digitalWrite(13, HIGH);
          delay(1000);
          digitalWrite(13, LOW);
        }

        Serial.write(bank);
        Serial.write(addr_upper);
        Serial.write(addr_lower);

        //Read the header
        readHeader();

        digitalWrite(snesReadPin, LOW);
        digitalWrite(snesCartPin, LOW);
        digitalWrite(snesResetPin, HIGH);
        digitalWrite(snesWritePin, HIGH);
        setDataBusDirection(INPUT);
        

        unsigned int maddress = 0;
        maddress |= addr_upper << 8;
        maddress |= addr_lower;

        byte bread = 0;
        while (bread < 64) {
          address(bank, maddress + bread);

          byte bus_data = (PIND & ~0x03) | (PINB & 0x03);
          Serial.write(bus_data);
          
          bread++;
        }
          
        //Serial.flush();
      }
      break;
    }
  }
  digitalWrite(snesResetPin, LOW);
}

void dumpCart(byte banks) {
  //dump the cart
  for (byte b = 0; b < banks; b++) {
    unsigned int a = readOffset;
    while(true) {
      address(b, a);
      
      // wait for USART to become ready & write byte from data bus
      while ( !( UCSR0A & (1<<UDRE0)) );
      UDR0 = (PIND & ~0x03) | (PINB & 0x03);

      if (a == 0xFFFF) {
        break;
      }
      a++;
    }
  }
}

void address(byte b, unsigned int a) {
  //Set AddressLatchPin (Digital pin 10) low
  //Digital pin 10 is bit 2 of PORTB
  PORTB &= ~(B100);
  
  //shift out bank address
  SPDR = b;
  while(!(SPSR & (1<<SPIF)));
  
  //shift out upper address
  SPDR = a >> 8;
  while(!(SPSR & (1<<SPIF)));
  
  //shift out lower address
  SPDR = a;
  while(!(SPSR & (1<<SPIF)));
  
  //Set AddressLatchPin (Digital pin 10) high
  //Digital pin 10 is bit 2 of PORTB
  PORTB |= (B100);


  //Wait 4 cycles for things to settle
  //One cycle of 16MHz AVR = 62.5ns.
  //SNES SlowRom access time is 200ns (4 cycles = 250ns).  
  asm volatile (
    "nop" "\n\t"
    "nop" "\n\t"
    "nop" "\n\t"
    "nop" "\n\t"
  );
}

byte serialReadBlocking() {
  while(Serial.available() == 0);
  return Serial.read();
}

//Write a value out to the address bus
/*void address(byte bank, unsigned int addr) {
  PORTB &= ~(B100); //Set AddressLatchPin low
  SPI.transfer(bank); // shift out bank
  SPI.transfer(addr >> 8); // shift out address upper byte
  SPI.transfer(addr); // shift out address lower byte
  PORTB |= (B100); //Set AddressLatchPin high
}*/

//Read byte from data bus
byte cartRead(){
  //Digital pins 2-7 (PIND) are connected to snes data lines 2-7.
  //Digital pins 8 and 9 (PINB) are connected to data lines 0 and 1 respectively
  //This line of code takes the data from pins 2-7 and from 8&9 and combines them into one byte.
  //The resulting bye looks like this: (pin7 pin6 pin5 pin4 pin3 pin2 pin9 pin8)
  return (PIND & ~0x03) | (PINB & 0x03);
}

//Write byte to data bus
void cartWrite(byte data) {
  digitalWrite(8, bitRead(data, 0));
  digitalWrite(9, bitRead(data, 1));
  for (int i = 2; i < 8; i++) {
    digitalWrite(i, bitRead(data, i));
  }
}

//Pull cart header
byte* getHeader() {
  digitalWrite(snesReadPin, LOW);
  digitalWrite(snesCartPin, LOW);
  digitalWrite(snesResetPin, HIGH);
  digitalWrite(snesWritePin, HIGH);
  
  byte buff[64];
  for (int i = 0; i < 64; i++) {
    address(0x00, 0xffc0 + i);
    buff[i] = cartRead();
  }
  return buff;
}

//get and then decode cart header
void readHeader() {
  byte* buff = getHeader();
  
  //ROM Layout: 0xffd5
  //The first bit of byte at address 0xffd5 indicates LoROM (0) or HiROM (1)
  romLayout = (bitRead(buff[21], 0) ? HiROM : LoROM);
  readOffset = (romLayout == HiROM ? 0x00 : 0x8000);
  
  //Cartridge type: 0xffd6
  cartType = buff[22];
  
  //ROM size: 0xffd7
  romSizeCode = buff[23];
  romSize = 1<<buff[23];
  romSize *= 1024;
  
  //RAM Size: 0xffd8
  ramSizeCode = buff[24];
  ramSize = (1 << ramSizeCode) * 1024;
  if (romSizeCode == 0) { romSize = 0; }
  if (ramSizeCode == 0) { ramSize = 0; }
}

//Set the data bus to output or input
void setDataBusDirection(bool dir) {
  for (int p = 2; p <=9; p++){
    pinMode(p, dir);
  }
}
