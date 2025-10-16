#pragma once

#include <Arduino.h>
#include <AsyncATHandler.h>
#include <Client.h>

#include "EG915/EG915.h"
#include "GSMTransport.h"
#include "GSMContext/GSMContext.h"

class AsyncGSM : public Client {
private:
  SemaphoreHandle_t rxMutex = xSemaphoreCreateMutex();
  GSMTransport transport;
  GSMContext *ctx = nullptr; // optional attached shared context

public:
  AsyncEG915U modem;
  AsyncATHandler at;

  AsyncGSM();
  ~AsyncGSM();
  virtual bool init(Stream &stream);
  bool begin(const char *apn = nullptr);
  // Attach to a shared GSMContext (no ownership).
  bool attach(GSMContext &context) {
    ctx = &context;
    return true;
  }
  int connect(IPAddress ip, uint16_t port) override;
  int connect(const char *host, uint16_t port) override;
  size_t write(uint8_t c) override;
  size_t write(const uint8_t *buf, size_t size) override;
  int available() override;
  int read() override;
  int read(uint8_t *buf, size_t size) override;
  int peek() override;
  void flush() override;
  void stop() override;
  virtual uint8_t connected() override;
  operator bool() override { return connected(); }

  // Information methods
  String getSimCCID();
  String getIMEI();
  String getOperator();
  String getIPAddress();

  // Connection management
  bool isConnected();
  bool gprsDisconnect();

  // helpers
  AsyncATHandler &getATHandler() { return at; }
  AsyncEG915U &getModem() { return modem; }
  // When attached, prefer context-owned instances
  AsyncATHandler &AT();
  AsyncEG915U &MODEM();
  GSMTransport &TRANSPORT();

protected:
  bool ssl = false;

  virtual bool modemConnect(const char *host, uint16_t port);
  virtual bool modemStop();
  int8_t getRegistrationStatusXREG(const char *regCommand);
  RegStatus getRegistrationStatus();

  // Data parsing
  bool parseQIRDResponse(const String &response);
};
