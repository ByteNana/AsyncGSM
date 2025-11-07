#pragma once

#include <Arduino.h>
#include <AsyncATHandler.h>

#include "SIMCard.settings.h"

class EG915SIMCard {
 public:
  EG915SIMCard() = default;
  explicit EG915SIMCard(AsyncATHandler &handler);

  void init(AsyncATHandler &handler);
  EG915SimDetConfig getDetection();
  EG915SimStatus getStatus();
  bool setStatusReport(bool enable);

  EG915SimSlot getCurrentSlot();
  bool setSlot(EG915SimSlot slot);

 private:
  AsyncATHandler *at{nullptr};
  EG915SimSlot currentSlot{EG915SimSlot::UNKNOWN};
};
