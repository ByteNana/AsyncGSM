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
};
