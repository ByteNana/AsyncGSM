#pragma once

#include "ModemBackend.h"

enum ModemType { EG915U, SIM7080 };

/* Modem is just an interface which on creation*/
class Modem {
 private:
  ModemBackend *backend = nullptr;
  ModemType Type;

 public:
  Modem(ModemType type) {
    switch (type) {
      case EG915U:
        backend = new EG915ModemBackend();
        break;
      case SIM7080:
        // backend = new SIM7080ModemBackend();
        break;
    }
    Type = type;
  }

  ModemType getType() const { return Type; }
  void setType(ModemType t) { Type = t; }

  bool init(Stream &stream, AsyncATHandler &atHandler, GSMTransport &transport) {
    return backend->init(stream, atHandler, transport);
  }
  bool setEchoOff() { return backend->setEchoOff(); }
  bool enableVerboseErrors() { return backend->enableVerboseErrors(); }
  bool checkModemModel() { return backend->checkModemModel(); }
  bool checkTimezone() { return backend->checkTimezone(); }
  bool checkSIMReady() { return backend->checkSIMReady(); }
  bool disalbeSleepMode() { return backend->disalbeSleepMode(); }
  bool gprsConnect(const char *apn, const char *user = nullptr, const char *pass = nullptr) {
    return backend->gprsConnect(apn, user, pass);
  }
  bool gprsDisconnect() { return backend->gprsDisconnect(); }
  String getSimCCID() { return backend->getSimCCID(); }
  String getIMEI() { return backend->getIMEI(); }
  String getIMSI() { return backend->getIMSI(); }
  String getOperator() { return backend->getOperator(); }
  String getIPAddress() { return backend->getIPAddress(); }
  bool connect(const char *host, uint16_t port) { return backend->connect(host, port); }
  bool stop() { return backend->stop(); }
  bool connectSecure(const char *host, uint16_t port) { return backend->connectSecure(host, port); }
  bool connectSecure(const char *host, uint16_t port, const char *caCertPath) {
    return backend->connectSecure(host, port, caCertPath);
  }
  bool stopSecure() { return backend->stopSecure(); }
  bool setSIMSlot(SimSlot slot) { return backend->setSIMSlot(slot); }

  bool uploadUFSFile(
      const char *path, const uint8_t *data, size_t size, uint32_t timeoutMs = 120000) {
    return backend->uploadUFSFile(path, data, size, timeoutMs);
  }
  bool setCACertificate(const char *ufsPath, const char *ssl_cidx) {
    return backend->setCACertificate(ufsPath, ssl_cidx);
  }
  bool findUFSFile(const char *pattern, String *outName = nullptr, size_t *outSize = nullptr) {
    return backend->findUFSFile(pattern, outName, outSize);
  }

  // Helpers for GPRS connection
  void disableConnections() { backend->disableConnections(); }
  bool setPDPContext(const char *apn) { return backend->setPDPContext(apn); }
  bool activatePDPContext() { return backend->activatePDPContext(); }
  bool isGPRSSAttached() { return backend->isGPRSSAttached(); }
  bool checkNetworkContext() { return backend->checkNetworkContext(); }
  bool activatePDP() { return backend->activatePDP(); }
  bool attachGPRS() { return backend->attachGPRS(); }
};