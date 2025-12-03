#pragma once

#include <Arduino.h>
#include <AsyncATHandler.h>
#include <Client.h>
#include <modules/EG915/EG915.h>
#include <utils/GSMTransport/GSMTransport.h>

static constexpr EG915SimSlot DEFAULT_SIM_SLOT = EG915SimSlot::SLOT_1;

class GSMContext {
 public:
  GSMContext();

  bool begin(Stream &stream, EG915SimSlot simSlot = DEFAULT_SIM_SLOT);
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
  EG915SimSlot simSlot{DEFAULT_SIM_SLOT};
};
