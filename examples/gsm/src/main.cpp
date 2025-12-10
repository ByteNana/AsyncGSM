#include <Arduino.h>

#include "AsyncGSM.h"
#include "modules/EG915/EG915.h"

AsyncEG915U eg915;
AsyncGSM modem;
HardwareSerial SerialAT(1);

void setup() {
  Serial.begin(115200);
  SerialAT.begin(115200);
  modem.context().begin(SerialAT, eg915);
  modem.context().setupNetwork("your_apn_here");
}

void loop() {}