/*
  LoRa Simple Gateway/Node Exemple

  This code uses InvertIQ function to create a simple Gateway/Node logic.

  Gateway - Sends messages with enableInvertIQ()
          - Receives messages with disableInvertIQ()

  Node    - Sends messages with disableInvertIQ()
          - Receives messages with enableInvertIQ()

  With this arrangement a Gateway never receive messages from another Gateway
  and a Node never receive message from another Node.
  Only Gateway to Node and vice versa.

  This code receives messages and sends a message every second.

  InvertIQ function basically invert the LoRa I and Q signals.

  See the Semtech datasheet, http://www.semtech.com/images/datasheet/sx1276.pdf
  for more on InvertIQ register 0x33.

  created 05 August 2018
  by Luiz H. Cassettari
*/

#include <SPI.h>               // include libraries
#include <LoRa.h>
#include <Wire.h>
#include <U8g2lib.h>           // Include the U8g2lib library
#include <SimpleEncoder.h>
#include <EEPROM.h>

#define Gateway 4              // Gateway Input Pin

const long frequency = 43352E4;// LoRa Frequency

const int csPin = 10;          // LoRa radio chip select
const int resetPin = 9;        // LoRa radio reset
const int irqPin = 2;          // change for your board; must be a hardware interrupt pin
bool MessageOk = false;
bool Selected = false;
bool NoFirstData = true;
bool DisplayOn = false;
unsigned long counter = 0;
unsigned long TimeInterval = 500;
String message = "";
unsigned int menuItem = 2;
int MenuVar[6] = {0,0,0,0,0,20};
int eeAddress; //EEPROM address to start reading from


void LoRa_rxMode();                    // function declarations
void LoRa_txMode();
void LoRa_sendMessage(String);
void onReceive(int);
void onTxDone();
boolean runEvery(unsigned long);
void Refresh_Display(void);
void ChekBotonera(int);
int ReadEEPROM(int); 
void WriteEEPROM(int, int);

const int BTN = 7;
const int encA = 5;
const int encB = 6;

SimpleEncoder encoder(BTN, encA, encB);

U8X8_SH1106_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE);


void setup() {
  pinMode(Gateway, INPUT_PULLUP);
  digitalWrite(Gateway, HIGH);
  Serial.begin(9600);                   // initialize serial
  u8x8.begin();  // initialize with the I2C
  u8x8.setPowerSave(0);
  // init done
  u8x8.setFont(u8x8_font_pxplustandynewtv_r);  // u8x8_font_torussansbold8_r, u8x8_font_chroma48medium8_r, u8x8_font_victoriamedium8_r, u8x8_font_pxplustandynewtv_r
  u8x8.draw1x2String(0,0,"Pimax s.r.l.");
  u8x8.setCursor(0,2);             // Column, Row
  u8x8.print("Pimax Remote");
  while (!Serial);
  LoRa.setPins(csPin, resetPin, irqPin);
  if (!LoRa.begin(frequency)) {
    u8x8.setCursor(0,3);             // Column, Row
    u8x8.print("LoRa init failed");
    while (true);                       // if failed, do nothing
  }
  u8x8.setCursor(0,3);             // Column, Row
  u8x8.print("LoRa succeeded");
  delay(1000);
  u8x8.clear();
  if (digitalRead(Gateway)) {
    u8x8.setCursor(0,0);             // Column, Row
    u8x8.print("LoRa Remoto On");
  } else {
    u8x8.setCursor(0,0);             // Column, Row
    u8x8.print("LoRa Equipo On");
    for (int i = 0; i < 6; i++){
      MenuVar[i] = ReadEEPROM(i);
    }
    delay(500);
    Refresh_Display();
  }
  delay(500);
  LoRa.onReceive(onReceive);
  LoRa.onTxDone(onTxDone);
  LoRa_rxMode();
}

void loop() {
  if (digitalRead(Gateway)){
    if (encoder.CLOCKWISE) {
      ChekBotonera(1);
    }
    if (encoder.COUNTERCLOCKWISE) {
      ChekBotonera(2);
    }
    if (encoder.BUTTON_PRESSED) {
      ChekBotonera(3);
    }
  }

  if (runEvery(TimeInterval) && digitalRead(Gateway)) {
    message = "HS";
    LoRa_sendMessage(message); // send a message
    u8x8.setCursor(0,1);             // Column, Row
    u8x8.print("Sin Conexion    ");
    TimeInterval = 500;
  }
  if (millis() - counter > 5500 && !digitalRead(Gateway)) {
    u8x8.setCursor(0,1);             // Column, Row
    u8x8.print("Sin Conexion    ");
    counter = millis();
  }

  if (MessageOk && !digitalRead(Gateway)) {   // Gateway Use
    // If first 2 characters received are HS, send a message back
    if (message.substring(0, 2) == "HS") {
      LoRa_sendMessage("ACK");
      String RSSI = String(LoRa.packetRssi());
      u8x8.setCursor(0,1);             // Column, Row
      u8x8.print("Conectado S" + RSSI + " ");
      if (NoFirstData) {
        delay(500);
        for(int i = 7; i >= 2; i--) {
          LoRa_sendMessage("MI" + String(i) + String(Selected) + String(MenuVar[i - 2]));
          delay(500);
        }
        delay(500);
        Refresh_Display();
        NoFirstData = false;
      }
      message = "";
    }
    // Normal Read for Gateway
    if (message.substring(0, 2) == "MI") {
      menuItem = message.substring(2, 3).toInt();
      MenuVar[menuItem - 2] = message.substring(4).toInt();
      WriteEEPROM(menuItem - 2, MenuVar[menuItem - 2]);
      if (message.substring(3, 4).toInt() == 1) Selected = true; else Selected = false;
      Refresh_Display();
      message = "";
    }
    if (message.substring(0, 2) == "RI") {
      message = "";
      NoFirstData = true;
    }
    counter = millis();
    MessageOk = false;
  }  
  if (MessageOk && digitalRead(Gateway)) {    // Node Use
    // If first 3 characters received are ACK, Hand Shake Ok
    if (message.substring(0, 3) == "ACK") {
      String RSSI = String(LoRa.packetRssi());
      TimeInterval = 5000;
      u8x8.setCursor(0,1);             // Column, Row
      u8x8.print("Conectado S" + RSSI + " ");
      if (!DisplayOn) LoRa_sendMessage("RI");
      message = "";
    }
    // Normal Read for Node
    if (message.substring(0, 2) == "MI") {
      menuItem = message.substring(2, 3).toInt();
      MenuVar[menuItem - 2] = message.substring(4).toInt();
      if (message.substring(3, 4).toInt() == 1) Selected = true; else Selected = false;
      Refresh_Display();
      message = "";
      NoFirstData = false;
    }
    counter = millis();
    MessageOk = false;
  }
}

void LoRa_rxMode(){
  if (digitalRead(Gateway)) {
    LoRa.enableInvertIQ();              // active invert I and Q signals
  } else {
    LoRa.disableInvertIQ();             // normal mode
  }
  LoRa.receive();                       // set receive mode
}

void LoRa_txMode(){
  LoRa.idle();                          // set standby mode
  if (digitalRead(Gateway)) {
    LoRa.disableInvertIQ();             // normal mode
  } else {
    LoRa.enableInvertIQ();              // active invert I and Q signals
  }
}

void LoRa_sendMessage(String message) {
  LoRa_txMode();                        // set tx mode
  LoRa.beginPacket();                   // start packet
  LoRa.print(message);                  // add payload
  LoRa.endPacket(true);                 // finish packet and send it
}

void onReceive(int packetSize) {
  message = "";
  while (LoRa.available()) {
    message += (char)LoRa.read();
  }
  MessageOk = true;
}

void onTxDone() {
  // Serial.println("TxDone");
  LoRa_rxMode();
}

boolean runEvery(unsigned long interval)
{
  static unsigned long previousMillis = 0;
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;
    return true;
  }
  return false;
}

void Refresh_Display(void){
  // u8x8.clear();
  counter = millis();
  u8x8.setCursor(15 ,0);              // Column, Row
  if (!NoFirstData) {
    u8x8.print("C");
  } else {
    u8x8.print("E");
  }
  u8x8.setCursor(0 ,2);              // Column, Row
  u8x8.print("  Power   "); if (MenuVar[0] < 1) u8x8.print("OFF"); else u8x8.print("ON "); 
  u8x8.setCursor(0 ,3);              // Column, Row
  u8x8.print("  Item 2  "); u8x8.print(MenuVar[1]); u8x8.print("  ");
  u8x8.setCursor(0 ,4);              // Column, Row
  u8x8.print("  Item 3  "); u8x8.print(MenuVar[2]); u8x8.print("  ");
  u8x8.setCursor(0 ,5);              // Column, Row
  u8x8.print("  Item 4  "); u8x8.print(MenuVar[3]); u8x8.print("  ");
  u8x8.setCursor(0 ,6);              // Column, Row
  u8x8.print("  Item 5  "); u8x8.print(MenuVar[4]); u8x8.print("  ");
  u8x8.setCursor(0 ,7);              // Column, Row
  u8x8.print("  Item 6  "); u8x8.print(MenuVar[5]); u8x8.print("  ");
  u8x8.setCursor(0 ,menuItem);              // Column, Row
  if (Selected) u8x8.print(">-"); else u8x8.print(" -");
  while(encoder.BUTTON_PRESSED);
  DisplayOn = true;
}

void ChekBotonera(int key){
switch(key){
  case 1:
    if (Selected) {
      if (menuItem == 2) {
        if (MenuVar[0] <= 0) MenuVar[0] = 1; 
      } else MenuVar[menuItem - 2] += 1; 
    }
    else{
      if (menuItem < 7) menuItem++; else menuItem = 2;
    }
    Refresh_Display();
    LoRa_sendMessage("MI" + String(menuItem) + String(Selected) + String(MenuVar[menuItem - 2]));
    break;

  case 2:
    if (Selected) {
      if (menuItem == 2){
        if (MenuVar[0] >= 1) MenuVar[0] = 0;
      } else MenuVar[menuItem - 2] -= 1; 
    }
    else{
      if (menuItem > 2) menuItem--; else menuItem = 7;
    }
    Refresh_Display();
    LoRa_sendMessage("MI" + String(menuItem) + String(Selected) + String(MenuVar[menuItem - 2]));
    break;

  case 3:
    if (Selected) Selected = false; else Selected = true;
    Refresh_Display();
    LoRa_sendMessage("MI" + String(menuItem) + String(Selected) + String(MenuVar[menuItem - 2]));
    break;

  default:
    // error = true;
    break;
  }
}

int ReadEEPROM(int i){
  int Data;
  eeAddress = sizeof(int) * i; 
  //Get the int data from the EEPROM at position 'eeAddress'
  EEPROM.get(eeAddress, Data);
  return Data;
}

void WriteEEPROM(int i, int Data){
  eeAddress = sizeof(int) * i; 
  EEPROM.put(eeAddress, Data);
}
