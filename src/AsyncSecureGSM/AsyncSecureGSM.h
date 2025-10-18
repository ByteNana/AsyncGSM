#pragma once

#include "AsyncGSM.h"

class AsyncSecureGSM : public AsyncGSM {
 public:
  AsyncSecureGSM(GSMContext &context);
  AsyncSecureGSM();

  void setCACert(const char *rootCA);

  GSMContext &context() { return (*ctx); }

 protected:
  bool isSecure() const override { return true; }
  bool modemConnect(const char *host, uint16_t port) override;
  bool modemStop() override;
};
