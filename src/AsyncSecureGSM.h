#pragma once

#include "AsyncGSM.h"

class AsyncSecureGSM : public AsyncGSM {
public:
  AsyncSecureGSM() { ssl = true; }

  bool setCertificate(const String &certificateName) {
    // return at->setCertificate(certificateName, mux);
    return true;
  }

protected:
  bool modemConnect(const char *host, uint16_t port) override;
  bool modemStop() override;
};
