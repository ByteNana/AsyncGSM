#pragma once

#include <Arduino.h>
#include <AsyncATHandler.h>
#include <Client.h>

enum BG96RegStatus {
  REG_NO_RESULT    = -1,
  REG_UNREGISTERED = 0,
  REG_SEARCHING    = 2,
  REG_DENIED       = 3,
  REG_OK_HOME      = 1,
  REG_OK_ROAMING   = 5,
  REG_UNKNOWN      = 4,
};

class AsyncGSM : public Client {
private:
  uint8_t _connected;

public:
  bool init(Stream &stream);
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
  uint8_t connected() override;
  operator bool() override { return true; }
  String getSimCCID();
  String getIMEI();
  String getOperator();
  String getIPAddress();
  bool isConnected();
  bool gprsDisconnect();
  bool gprsConnect(const char *apn, const char *user = nullptr,
                   const char *pwd = nullptr);

protected:
  AsyncATHandler at;
  bool modemConnect(const char *host, uint16_t port);
  int8_t getRegistrationStatusXREG(const char* regCommand);
  BG96RegStatus getRegistrationStatus();
};
