#include "AsyncGSM.h"

#include "esp_log.h"

bool AsyncGSM::isConnected() {
  RegStatus s = ctx->modem().network().getRegistrationStatus();
  return (s == RegStatus::REG_OK_HOME || s == RegStatus::REG_OK_ROAMING);
}