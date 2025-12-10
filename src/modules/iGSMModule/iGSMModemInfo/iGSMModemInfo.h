#pragma once

#include <Arduino.h>

class iModemInfo {
 public:
  virtual ~iModemInfo() = default;
  virtual String getModel() = 0;
  virtual String getManufacturer() = 0;
  virtual String getIMEI() = 0;
  virtual String getIMSI() = 0;
  virtual String getICCID() = 0;
  virtual String getIPAddress() = 0;
};
