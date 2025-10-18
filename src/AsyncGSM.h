#pragma once

#include <Arduino.h>
#include <AsyncATHandler.h>
#include <Client.h>
#include <GSMContext/GSMContext.h>
#include <modules/EG915/EG915.h>
#include <utils/GSMTransport/GSMTransport.h>

class AsyncGSM : public Client {
 private:
  SemaphoreHandle_t rxMutex = xSemaphoreCreateMutex();

 public:
  AsyncGSM(GSMContext &context);
  AsyncGSM();
  ~AsyncGSM();

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

  GSMContext &context() { return (*ctx); }

 protected:
  bool owns = false;
  GSMContext *ctx;
  virtual bool isSecure() const { return false; }

  virtual bool modemConnect(const char *host, uint16_t port);
  virtual bool modemStop();
  int8_t getRegistrationStatusXREG(const char *regCommand);
  RegStatus getRegistrationStatus();

  // Data parsing
  bool parseQIRDResponse(const String &response);
};
