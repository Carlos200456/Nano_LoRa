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

#include <SPI.h>              // include libraries
#include <LoRa.h>
#define Scopia 4

const long frequency = 432E6;  // LoRa Frequency

const int csPin = 10;          // LoRa radio chip select
const int resetPin = 9;        // LoRa radio reset
const int irqPin = 2;          // change for your board; must be a hardware interrupt pin

int counter = 0;
char SCin = 0;
unsigned long lastDebounceTimeSC = 0;  // the last time the output SC was toggled
unsigned long debounceDelaySC = 20;    // the debounce time; increase if the output flickers
unsigned long Presed = 0;
int buttonStateSC;                     // the current reading from the input pin
int lastButtonStateSC = 0;           // the previous reading from the input pin SC
String message = "";
bool FluoroOn = false;

void LoRa_rxMode();            // function declarations
void LoRa_txMode();
void LoRa_sendMessage(String message);
void onReceive(int packetSize);
void onTxDone();
boolean runEvery(unsigned long interval);

void setup() {
  pinMode(Scopia, INPUT_PULLUP);
  digitalWrite(Scopia, HIGH);
  Serial.begin(9600);                   // initialize serial
  while (!Serial);

  LoRa.setPins(csPin, resetPin, irqPin);

  if (!LoRa.begin(frequency)) {
    Serial.println("LoRa init failed. Check your connections.");
    while (true);                       // if failed, do nothing
  }

  Serial.println("LoRa init succeeded.");
  Serial.println();
  Serial.println("LoRa Simple Node");
  Serial.println("Only receive messages from gateways");
  Serial.println("Tx: invertIQ disable");
  Serial.println("Rx: invertIQ enable");
  Serial.println();

  LoRa.onReceive(onReceive);
  LoRa.onTxDone(onTxDone);
  LoRa_rxMode();
}

void loop() {
// Fluoro Input ------------------------------
  SCin = digitalRead(Scopia);
  if (SCin != lastButtonStateSC) {
    // reset the debouncing timer
    lastDebounceTimeSC = millis();
  }
  if ((millis() - lastDebounceTimeSC) > debounceDelaySC) {        // Demora para empesar a pulsar en Cine
    // whatever the reading is at, it's been there for longer
    // than the debounce delay, so take it as the actual current state:
    // if the button state has changed:
    if (SCin != buttonStateSC) {
      buttonStateSC = SCin;
      // only toggle the FluoroOn if the new Scopia button state is LOW
      if (buttonStateSC == HIGH) {
        message = "FluoroOff";
        FluoroOn = true;
        counter++;
      }else {
        message = "FluoroOn";
        FluoroOn = true;
        counter++;
      }
    }
  }
  
  lastButtonStateSC = SCin;


  if (FluoroOn) {
    LoRa_sendMessage(message); // send a message
    FluoroOn = false;
    Serial.print("Send Message: ");
    Serial.println(message);
  }
}

void LoRa_rxMode(){
  LoRa.enableInvertIQ();                // active invert I and Q signals
  LoRa.receive();                       // set receive mode
}

void LoRa_txMode(){
  LoRa.idle();                          // set standby mode
  LoRa.disableInvertIQ();               // normal mode
}

void LoRa_sendMessage(String message) {
  LoRa_txMode();                        // set tx mode
  LoRa.beginPacket();                   // start packet
  LoRa.print(message);                  // add payload
  LoRa.endPacket(true);                 // finish packet and send it
}

void onReceive(int packetSize) {
  String message = "";

  while (LoRa.available()) {
    message += (char)LoRa.read();
  }

  Serial.print("Node Receive: ");
  Serial.println(message);
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
