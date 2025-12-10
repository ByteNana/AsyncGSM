#include "Socket.h"

#include "esp_log.h"

EG915Socket::EG915Socket() : at(nullptr) {}

void EG915Socket::init(AsyncATHandler &handler) { at = &handler; }

bool EG915Socket::connect(const char *host, uint16_t port) {
  if (!at) {
    log_e("AT handler not initialized");
    return false;
  }

  String portStr(port);
  isConnected.store(ConnectionStatus::DISCONNECTED);
  at->sendSync(String("AT+QIOPEN=1,0,\"TCP\",\"") + host + "\"," + portStr + ",0,0");

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

size_t EG915Socket::sendData(const uint8_t *buf, size_t size, bool isSecure) {
  if (!at) {
    log_e("AT handler not initialized");
    return 0;
  }
  if (isConnected.load() != ConnectionStatus::CONNECTED || !at->getStream()) {
    log_e("Not connected or stream not initialized");
    return 0;
  }

  if (size == 0) return 0;
  log_d("Writing %zu bytes to modem...", size);

  String command = isSecure ? "AT+QSSLSEND=0," : "AT+QISEND=0,";

  ATPromise *promise = at->sendCommand(command + String(size));
  if (!promise) {
    log_e("Failed to create promise for SEND");
    at->popCompletedPromise(promise->getId());
    return 0;
  }

  while (!at->getStream()->available()) { vTaskDelay(0); }
  String response;
  while (at->getStream()->available()) {
    char c = at->getStream()->read();
    response += c;
    if (c == '>') { break; }
  }
  if (response.indexOf('>') == -1) {
    log_e("Did not receive prompt '>'");
    at->popCompletedPromise(promise->getId());
    return 0;
  }

  at->getStream()->write(buf, size);
  at->getStream()->flush();

  bool promptReceived = promise->expect("SEND OK")->wait();

  auto p = at->popCompletedPromise(promise->getId());
  if (!promptReceived) {
    log_e("Failed to get SEND OK confirmation");
    return 0;
  }

  log_d("Write successful.");
  return size;
}

bool EG915Socket::stop() {
  if (!at) {
    log_e("AT handler not initialized");
    return false;
  }
  // Request close
  bool ok = at->sendSync("AT+QICLOSE=0");
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

ConnectionStatus EG915Socket::getConnectionStatus() { return isConnected.load(); }
