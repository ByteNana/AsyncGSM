#pragma once

#include <Arduino.h>
#include <AsyncATHandler.h>
#include <Client.h>
#include <modules/iGSMModule/iGSMModule.h>
#include <utils/GSMTransport/GSMTransport.h>

class GSMContext {
 public:
  GSMContext();

  bool begin(Stream &stream, iGSMModule &module, SIMSlot simSlot = SIMSlot::SLOT_1);
  bool setupNetwork(const char *apn);
  void end();

  AsyncATHandler &at() { return atHandler; }
  iGSMModule &modem() { return *modemDriver; }
  bool hasModule() const { return modemDriver != nullptr; }
  GSMTransport &transport() { return rxTransport; }
  Stream *stream() { return ioStream; }

 private:
  SemaphoreHandle_t rxMutex;
  GSMTransport rxTransport;
  AsyncATHandler atHandler;
  iGSMModule *modemDriver = nullptr;
  Stream *ioStream = nullptr;
  SIMSlot simSlot;
};