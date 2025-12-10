#pragma once

#include <Arduino.h>

#include "iGSMNetwork.types.h"

class iGSMNetwork {
 public:
  virtual ~iGSMNetwork() = default;
  virtual RegStatus getRegistrationStatus() = 0;
  virtual String getOperator() = 0;
  virtual bool isGPRSSAttached() = 0;
  virtual bool gprsConnect(
      const char *apn, const char *user = nullptr, const char *pass = nullptr) = 0;
  virtual bool gprsDisconnect() = 0;
  virtual bool setPDPContext(const char *apn) = 0;
  virtual bool activatePDP() = 0;
  virtual bool checkContext() = 0;
  virtual bool attachGPRS() = 0;

  virtual void setRegistrationStatus(RegStatus status) = 0;

 protected:
  std::atomic<RegStatus> creg = RegStatus::REG_NO_RESULT;
};
