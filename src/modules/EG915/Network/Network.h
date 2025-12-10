#pragma once

#include <Arduino.h>
#include <AsyncATHandler.h>
#include <modules/iGSMModule/iGSMNetwork/iGSMNetwork.h>

class EG915Network : public iGSMNetwork {
 public:
  EG915Network() = default;
  explicit EG915Network(AsyncATHandler &handler);

  void init(AsyncATHandler &handler);
  RegStatus getRegistrationStatus() override;
  String getOperator() override;
  bool isGPRSSAttached() override;
  bool gprsConnect(
      const char *apn, const char *user = nullptr, const char *pass = nullptr) override;
  bool gprsDisconnect() override;
  bool activatePDP() override;
  bool setPDPContext(const char *apn) override;
  bool checkContext() override;
  bool attachGPRS() override;

  void setRegistrationStatus(RegStatus status) override;

 private:
  AsyncATHandler *at = nullptr;
};
