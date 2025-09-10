#include "AsyncSecureGSM.h"
#include "esp_log.h"

bool AsyncSecureGSM::modemConnect(const char *host, uint16_t port) {
  return modem.connectSecure(host, port);
}
