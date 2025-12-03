#include <Arduino.h>

#include "AsyncGSM.h"

AsyncGSM modem;
HardwareSerial SerialAT(1);

void setup() {
  Serial.begin(115200);
  SerialAT.begin(115200);
  modem.context().begin(SerialAT);
  modem.context().setupNetwork("your_apn_here");
}

void loop() {}
