#pragma once

#include <Arduino.h>

#include "iSIMCard.types.h"

class iSIMCard {
 public:
  virtual ~iSIMCard() = default;

  virtual SIMDetectionConfig getDetection() = 0;
  virtual SIMStatusReport getStatus() = 0;
  virtual bool setStatusReport(bool enable) = 0;

  virtual SIMSlot getCurrentSlot() = 0;
  virtual bool setSlot(SIMSlot slot) = 0;
  virtual bool isReady() = 0;
};
