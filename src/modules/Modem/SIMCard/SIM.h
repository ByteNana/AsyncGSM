#pragma once

#include <Arduino.h>
#include <AsyncATHandler.h>

#include "SIM.settings.h"

class SIMCard {
 public:
  SIMCard() = default;
  explicit SIMCard(AsyncATHandler &handler);

  void init(AsyncATHandler &handler);
  SimDetConfig getDetection();
  SimStatus getStatus();
  bool setStatusReport(bool enable);

  SimSlot getCurrentSlot();
  bool setSlot(SimSlot slot);

 private:
  AsyncATHandler *at{nullptr};
  SimSlot currentSlot{SimSlot::UNKNOWN};
};
