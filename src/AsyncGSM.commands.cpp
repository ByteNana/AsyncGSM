#include "AsyncGSM.h"
#include "esp_log.h"

bool AsyncGSM::isConnected() {
  RegStatus s = getRegistrationStatus();
  return (s == RegStatus::REG_OK_HOME || s == RegStatus::REG_OK_ROAMING);
}

bool AsyncGSM::modemConnect(const char *host, uint16_t port) {
  return modem.connect(host, port);
}
