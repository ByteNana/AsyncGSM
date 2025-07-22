#pragma once

#include <Arduino.h>
#include <AsyncATHandler.h>
#include <Client.h>

class AsyncGSM : Client {
public:
  bool init(Stream &stream);
  int connect(const char *host, uint16_t port) override;
  void stop() override;
  String getSimCCID();
  bool gprsDisconnect();
  bool gprsConnect(const char *apn, const char *user = nullptr,
                   const char *pwd = nullptr);

protected:
  AsyncATHandler at;
};
