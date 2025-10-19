#pragma once

#include <AsyncMqttGSM/AsyncMqttGSM.h>
#include <GSMContext/GSMContext.h>

class AsyncSecureMqttGSM : public AsyncMqttGSM {
 public:
  AsyncSecureMqttGSM(GSMContext &context) : AsyncMqttGSM(context) {}
  AsyncSecureMqttGSM() : AsyncMqttGSM() {}

  // Compute MD5 filename, upload if needed, and configure QSSLCFG cacert
  void setCACert(const char *rootCA);
  bool connect(const char *id, const char *user, const char *pass) override;
  bool publish(const char *topic, const uint8_t *payload, unsigned int plength) override;
  bool subscribe(const char *topic) override;

 private:
  String caMd5Cache;

  void setSecurityLevel(bool secure);

 protected:
  const char *ssl_cidx = "2";
  bool isSecure() const override { return true; }
};
