#pragma once

#include "AsyncGSM.h"

class AsyncSecureGSM : public AsyncGSM {
public:
  AsyncSecureGSM() { ssl = true; }

  void setCACert(const char *rootCA);

protected:
  bool modemConnect(const char *host, uint16_t port) override;
  bool modemStop() override;
};
