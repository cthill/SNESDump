/*
Christian Hill
Version 1.0
*/
#define NUMBER_OF_SHIFT_CHIPS   1
#define DATA_WIDTH   NUMBER_OF_SHIFT_CHIPS * 8
#define PULSE_WIDTH_USEC   1

const int AddressRegisterClockPin = A4; //clock
const int AddressSerialOutPin = A1; //output
const int AddressSerialClockPin = A3; //latch


const int snesCartPin = 10; //Cart pin 49 - aka /CS, /ROMSEL, /Cart - Goes low when reading ROM
const int snesWritePin = 11; //Cart pin 54 - aka /WR - Address bus A write strobe.
const int snesReadPin = 12; //Cart pin 23 - aka /RD - Address bus A read strobe.
const int snesResetPin = 13; //Cart pin 26 - SNES reset pin. Goes LOW on reset. High when reading cart

const int LoROM = 0;
const int HiROM = 1;

typedef enum Flags { 
  HEADRead,
  ROMRead,
  RAMRead,
  RAMWrite,
  INTTest,
};

const long bankSize = 0x10000; //65536 bytes

byte mode;
boolean done = false;
String gameTitle;
int romLayout;
byte cartType;
unsigned long readOffset;
unsigned long romSize;
unsigned long ramSize;
byte romSizeCode;
byte ramSizeCode;

void setup() {
  Serial.begin(1000000);
//Serial.begin(115200);
//Serial.begin(230400);
//Serial.begin(9600);
  pinMode(AddressRegisterClockPin, OUTPUT);
  pinMode(AddressSerialOutPin, OUTPUT);
  pinMode(AddressSerialClockPin, OUTPUT);
  
  //Set the data pins to either read or write
  int pinmode = (mode == RAMWrite) ? OUTPUT : INPUT;
  for (int p = 2; p <=9; p++){
    pinMode(p, pinmode);
  }
  
  pinMode(snesCartPin, OUTPUT);
  pinMode(snesWritePin, OUTPUT);
  pinMode(snesReadPin, OUTPUT);
  pinMode(snesResetPin, OUTPUT);
  
  readHeader();
}

void loop() {
  done = true;
  if (Serial.available() > 0) {
    int incomingByte = Serial.read();
    switch(incomingByte) {
      case HEADRead:
        byte* buff;
        buff = readHeader();
        Serial.write(buff, 64);
        //printHeader();
        break;
        
      case ROMRead:
      case RAMRead:
      case RAMWrite:
      case INTTest:
        mode = incomingByte;
        done = false;
        break;
    }
  }
  if(!done){
    int pinmode = (mode == RAMWrite) ? OUTPUT : INPUT;
    for (int p = 2; p <=9; p++){
      pinMode(p, pinmode);
    }
    
    switch (mode) {
      case INTTest:
      {
        unsigned int a = 0xFFF0;
          while(a++) {
            Serial.println(a);
            if (a == 0xFFFF) {
              break;
            }
          }
      }
      break;
      case ROMRead:
      {
        //Same for both high and low rom carts
        readHeader();
        Serial.write(romSizeCode);
        digitalWrite(snesReadPin, LOW);
        digitalWrite(snesCartPin, LOW);
        digitalWrite(snesResetPin, HIGH);
        digitalWrite(snesWritePin, HIGH);
        byte banks = romSize / (bankSize - readOffset);
        for (byte b = 0; b < banks; b++) {
          byte data[bankSize - readOffset];
          unsigned int a = readOffset;
          while(true) {
            address(b, a);
            Serial.write(cartRead());
            if (a == 0xFFFF) {
              break;
            }
            a++;
          }
        }
      }
      break;
      
      case RAMRead:
      {
        Serial.write(ramSizeCode);
        digitalWrite(snesReadPin, LOW);
        digitalWrite(snesCartPin, (romLayout == HiROM ? HIGH : LOW));
        digitalWrite(snesResetPin, HIGH);
        digitalWrite(snesWritePin, HIGH);
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
      }
      break;
        
      case RAMWrite:
      {
        Serial.write(ramSizeCode);
        digitalWrite(snesReadPin, HIGH);
        digitalWrite(snesCartPin, (romLayout == HiROM ? HIGH : LOW));
        digitalWrite(snesResetPin, HIGH);
        digitalWrite(snesWritePin, LOW);
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
    }
    mode = 0;
    done = true;
  }
  digitalWrite(snesResetPin, LOW);
}

void address(byte bank, unsigned int addr) {
  digitalWrite(AddressSerialClockPin, LOW); 
  shiftOut(AddressSerialOutPin, AddressRegisterClockPin, MSBFIRST, bank);
  shiftOut(AddressSerialOutPin, AddressRegisterClockPin, MSBFIRST, addr >> 8);
  shiftOut(AddressSerialOutPin, AddressRegisterClockPin, MSBFIRST, addr);
  digitalWrite(AddressSerialClockPin, HIGH);
}

byte cartRead(){
  //Digital pins 2-7 (PIND) are connected to snes data lines 2-7.
  //Digital pins 8 and 9 (PINB) are connected to data lines 0 and 1 respectively
  //This line of code takes the data from pins 2-7 and from 8&9 and combines them into one byte.
  //The resulting bye looks like this: (pin7 pin6 pin5 pin4 pin3 pin2 pin9 pin8)
  byte out = (PIND & ~0x03) | (PINB & 0x03);
  return out;
}

void cartWrite(byte data) {
  digitalWrite(8, bitRead(data, 0));
  digitalWrite(9, bitRead(data, 1));
  for (int i = 2; i < 8; i++) {
    digitalWrite(i, bitRead(data, i));
  /*digitalWrite(3, bitRead(data, 3));
  digitalWrite(4, bitRead(data, 4));
  digitalWrite(5, bitRead(data, 5));
  digitalWrite(6, bitRead(data, 6));
  digitalWrite(7, bitRead(data, 7));*/
  }
}

byte* readHeader() {
  digitalWrite(snesReadPin, LOW);
  digitalWrite(snesCartPin, LOW);
  digitalWrite(snesResetPin, HIGH);
  digitalWrite(snesWritePin, HIGH);
  
  byte buff[64];
  for (int i = 0; i < 64; i++) {
    address(0x00, 0xffc0 + i);
    buff[i] = cartRead();
  }
  
  gameTitle="";
  //Reading Cart Header addresses 0xffc0 and ends at 0xffff
  //Game title: 0xffc0 - 0xffd4
  for (unsigned long i = 0; i < 21; i++) {
    gameTitle += char(buff[i]);
  }
  //ROM Layout: 0xffd5
  //address(0x00, 0xffd5);
  //The first bit of byte at address 0xffd5 indicates LoROM (0) or HiROM (1)
  romLayout = (bitRead(buff[21], 0) ? HiROM : LoROM);
  readOffset = (romLayout == HiROM ? 0x00 : 0x8000);
  
  //Cartridge type: 0xffd6
  //address(0x00, 0xffd6);
  cartType = buff[22];
  
  //ROM size: 0xffd7
  //address(0x00, 0xffd7); 
  romSizeCode = buff[23];
  romSize = 1<<buff[23];
  romSize *= 1024;
  
  //RAM Size: 0xffd8
  //address(0x00, 0xffd8);
  ramSizeCode = buff[24];
  ramSize = (1 << ramSizeCode) * 1024;
  if (romSizeCode == 0) { romSize = 0; }
  if (ramSizeCode == 0) { ramSize = 0; }
  
  return buff;
}
