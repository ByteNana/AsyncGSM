#pragma once

#include <Arduino.h>
#include <AsyncATHandler.h>
#include <modules/iGSMModule/iSIMCard/iSIMCard.h>

class EG915SIMCard : public iSIMCard {
 public:
  EG915SIMCard() = default;
  explicit EG915SIMCard(AsyncATHandler &handler);

  void init(AsyncATHandler &handler);
  SIMDetectionConfig getDetection() override;
  SIMStatusReport getStatus() override;
  bool setStatusReport(bool enable) override;

  SIMSlot getCurrentSlot() override;
  bool setSlot(SIMSlot slot) override;
  bool isReady() override;

 private:
  AsyncATHandler *at = nullptr;
  SIMSlot currentSlot = SIMSlot::UNKNOWN;
};
