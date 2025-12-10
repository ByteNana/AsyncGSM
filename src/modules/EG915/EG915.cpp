#include "EG915.h"

#include <utils/GSMTransport/GSMTransport.h>

#include "esp_log.h"

AsyncEG915U::AsyncEG915U() {}

AsyncEG915U::~AsyncEG915U() {}

bool AsyncEG915U::init(Stream &stream, AsyncATHandler &atHandler, GSMTransport &transportRef) {
  at = &atHandler;
  _stream = &stream;
  transport = &transportRef;
  // Register dynamic URC handlers
  registerURCs();
  // Initialize submodules
  simModule.init(atHandler);
  networkModule.init(atHandler);
  modemInfoModule.init(atHandler);
  socketModule.init(atHandler);
  mqttModule.init(atHandler);
  return true;
}

bool AsyncEG915U::setEchoOff() { return at->sendSync("ATE0"); }

bool AsyncEG915U::enableVerboseErrors() { return at->sendSync("AT+CMEE=2"); }

bool AsyncEG915U::checkTimezone() { return at->sendSync("AT+CTZU=1"); }

void AsyncEG915U::disableConnections() {
  for (int i = 0; i < 4; i++) { at->sendSync(String("AT+QICLOSE=") + String(i)); }
}

bool AsyncEG915U::disalbeSleepMode() { return at->sendSync("AT+QSCLK=0"); }

bool AsyncEG915U::checkModemModel() {
  // TODO: Implement based on modemInfoModule
  return true;
}

bool AsyncEG915U::checkSIMReady() {
  // TODO: Implement based on simModule
  return simModule.isReady();
}

ConnectionStatus AsyncEG915U::getConnectionStatus() { return socketModule.getConnectionStatus(); }