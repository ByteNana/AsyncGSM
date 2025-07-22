#include <Arduino.h>
#include <HttpClient.h>

#include "AsyncGSM.h"

AsyncGSM modem;
HardwareSerial SerialAT(1);

const char server[] = "example.com";
const int port = 80;

HttpClient http(modem, server, port);

void setup() {
  Serial.begin(115200);
  SerialAT.begin(115200);
  modem.init(SerialAT);
  modem.begin("internet");
}

void loop() {
  int status = http.get("/");
  if (status == 200) {
    String body = http.responseBody();
    Serial.println(body);
  }
  delay(10000);
}
