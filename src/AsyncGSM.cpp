#include "AsyncGSM.h"
#include "esp_log.h"

bool AsyncGSM::init(Stream &stream) {
  if (!at.begin(stream)) {
    log_e("Failed to initialize AsyncATHandler");
    return false;
  }
  return true;
}

int AsyncGSM::connect(const char *host, uint16_t port) {
  // stop();
  // TINY_GSM_YIELD();
  // rx.clear();
  // sock_connected = at->modemConnect(host, port, mux, timeout_s);
  // return sock_connected;
  return 0;
}

void AsyncGSM::stop() {
  // uint32_t startMillis = millis();
  // dumpModemBuffer(maxWaitMs);
  // at->sendAT(GF("+QICLOSE="), mux);
  // sock_connected = false;
  // at->waitResponse((maxWaitMs - (millis() - startMillis)));
}
