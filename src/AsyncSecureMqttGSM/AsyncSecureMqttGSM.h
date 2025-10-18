#pragma once

#include <AsyncMqttGSM/AsyncMqttGSM.h>
#include <GSMContext/GSMContext.h>

class AsyncSecureMqttGSM : public AsyncMqttGSM {
 public:
  AsyncSecureMqttGSM(GSMContext &context) : AsyncMqttGSM(context) {}
  AsyncSecureMqttGSM() : AsyncMqttGSM() {}

  // Compute MD5 filename, upload if needed, and configure QSSLCFG cacert
  void setCACert(const char *rootCA);

 private:
  String caMd5Cache;

 protected:
  bool isSecure() const override { return true; }
};
