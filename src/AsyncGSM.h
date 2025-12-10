#pragma once

#include <Arduino.h>
#include <AsyncATHandler.h>
#include <Client.h>
#include <GSMContext/GSMContext.h>

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

  // Connection management
  bool isConnected();
  bool gprsDisconnect();

  GSMContext &context() { return (*ctx); }

 protected:
  bool owns = false;
  GSMContext *ctx;
  virtual bool isSecure() const { return false; }
};
