#include "Socket.h"

bool EG915Socket::connectSecure(const char *host, uint16_t port) {
  return connectSecure(host, port, nullptr);
}

bool EG915Socket::connectSecure(const char *host, uint16_t port, const char *caCertPath) {
  if (!at) {
    log_e("AT handler not initialized");
    return false;
  }
  // Enable TLS 1.2
  if (!at->sendSync("AT+QSSLCFG=\"sslversion\",1,3")) {
    log_e("Failed to set SSL version");
    return false;
  }

  // Allow a strong default cipher suite (RSA with AES-256-CBC-SHA)
  if (!at->sendSync("AT+QSSLCFG=\"ciphersuite\",1,0X0035")) {
    log_e("Failed to set SSL cipher suite");
    return false;
  }

  // Enable SNI so the server can select proper certificate when using hostnames
  at->sendSync("AT+QSSLCFG=\"sni\",1,1");

  // Set security level: 1 when CA is configured, else 0 (insecure)
  if (!at->sendSync(String("AT+QSSLCFG=\"seclevel\",1,") + (certConfigured ? "1" : "0"))) {
    log_e("Failed to set SSL security level");
    return false;
  }

  // Open SSL connection (PDP ctx=1, SSL ctx=1, clientid=0)
  isConnected.store(ConnectionStatus::DISCONNECTED);
  at->sendSync(String("AT+QSSLOPEN=1,1,0,\"") + host + "\"," + String(port) + ",0");

  for (int i = 0; i < 20; i++) {
    ConnectionStatus status = isConnected.load();
    if (status == ConnectionStatus::CONNECTED || status == ConnectionStatus::FAILED) { break; }
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  if (isConnected.load() == ConnectionStatus::CONNECTED) {
    log_d("Connection URC received successfully");
  } else {
    log_e("No +QIOPEN URC received or it indicated failure");
  }
  return isConnected.load() == ConnectionStatus::CONNECTED;
}

bool EG915Socket::stopSecure() {
  if (!at) {
    log_e("AT handler not initialized");
    return false;
  }
  // Request close
  bool ok = at->sendSync("AT+QSSLCLOSE=0");
  // Wait briefly for URC to arrive and update state; under heavy logs this can be delayed
  if (ok) {
    TickType_t t0 = xTaskGetTickCount();
    while (isConnected.load() != ConnectionStatus::DISCONNECTED &&
           (xTaskGetTickCount() - t0) < pdMS_TO_TICKS(2000)) {
      vTaskDelay(pdMS_TO_TICKS(5));
    }
    isConnected.store(ConnectionStatus::DISCONNECTED);
    if (transport) { transport->reset(); }
  }
  return ok;
}
