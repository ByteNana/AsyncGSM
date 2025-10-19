#pragma once

#include "AsyncGSM.h"

class AsyncSecureGSM : public AsyncGSM {
 public:
  AsyncSecureGSM(GSMContext &context);
  AsyncSecureGSM();

  void setCACert(const char *rootCA);

 protected:
  const char *ssl_cidx = "1";
  bool isSecure() const override { return true; }
  bool modemConnect(const char *host, uint16_t port) override;
  bool modemStop() override;
};
