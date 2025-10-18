#pragma once

#include <Arduino.h>
#include <AsyncATHandler.h>
#include <Client.h>
#include <modules/EG915/EG915.h>
#include <utils/GSMTransport/GSMTransport.h>

class GSMContext {
 public:
  GSMContext();

  bool begin(Stream &stream);
  bool setupNetwork(const char *apn);
  void end();

  AsyncATHandler &at() { return atHandler; }
  AsyncEG915U &modem() { return modemDriver; }
  GSMTransport &transport() { return rxTransport; }
  Stream *stream() { return ioStream; }

 private:
  SemaphoreHandle_t rxMutex;
  GSMTransport rxTransport;
  AsyncATHandler atHandler;
  AsyncEG915U modemDriver;
  Stream *ioStream{nullptr};
};
