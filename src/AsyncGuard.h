#pragma once

#include "freertos/FreeRTOS.h"
#include <Arduino.h>

static SemaphoreHandle_t asyncGuardMutex = xSemaphoreCreateMutex();
struct AsyncGuard {
  AsyncGuard() { xSemaphoreTake(asyncGuardMutex, portMAX_DELAY); }
  ~AsyncGuard() { xSemaphoreGive(asyncGuardMutex); }
};
