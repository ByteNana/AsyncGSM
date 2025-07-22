#pragma once

#include <Arduino.h>
#include <AsyncATHandler.h>
#include <Client.h>

#define TINY_GSM_MUX_COUNT 12

class AsyncGSM : Client {
public:
  bool init(Stream &stream);
  int connect(const char *host, uint16_t port) override;
  void stop() override;
  String getSimCCID();

protected:
  AsyncATHandler at;
  bool gprsDisconnect();
  bool gprsConnect(const char *apn, const char *user = nullptr,
                   const char *pwd = nullptr);
};
