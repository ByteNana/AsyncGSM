#pragma once

#include <Arduino.h>
#include <AsyncATHandler.h>
#include <Client.h>
#include "AsyncGuard.h"

#include "EG915/EG915.h"

class AsyncGSM : public Client {
private:
  std::deque<uint8_t> rxBuffer;
  SemaphoreHandle_t rxMutex = xSemaphoreCreateMutex();
  bool endOfDataReached;
  int consecutiveEmptyReads;

  void lockRx() { xSemaphoreTake(rxMutex, portMAX_DELAY); }
  void unlockRx() { xSemaphoreGive(rxMutex); }
public:
  AsyncEG915U modem;
  AsyncATHandler at;

  AsyncGSM();
  ~AsyncGSM();
  virtual bool init(Stream &stream);
  bool begin(const char *apn = nullptr);
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

protected:
  bool ssl = false;

  virtual bool modemConnect(const char *host, uint16_t port);
  virtual bool modemStop();
  int8_t getRegistrationStatusXREG(const char *regCommand);
  RegStatus getRegistrationStatus();

  // Data parsing
  bool parseQIRDResponse(const String &response);
};
