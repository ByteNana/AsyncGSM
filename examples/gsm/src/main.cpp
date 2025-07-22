#include <Arduino.h>

#include "AsyncGSM.h"

AsyncGSM modem;
HardwareSerial SerialAT(1);

void setup() {
  Serial.begin(115200);
  SerialAT.begin(115200);
  modem.init(SerialAT);
}

void loop() {}
