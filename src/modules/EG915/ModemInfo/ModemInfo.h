#pragma once

#include <Arduino.h>
#include <AsyncATHandler.h>
#include <modules/iGSMModule/iGSMModemInfo/iGSMModemInfo.h>

class EG915ModemInfo : public iModemInfo {
 public:
  EG915ModemInfo() = default;
  ~EG915ModemInfo() = default;
  void init(AsyncATHandler &handler) { at = &handler; }
  String getModel() override;
  String getManufacturer() override;
  String getIMEI() override;
  String getIMSI() override;
  String getICCID() override;
  String getIPAddress() override;

 private:
  AsyncATHandler *at = nullptr;
};
