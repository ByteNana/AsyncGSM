#include "AsyncGSM.h"

#include <algorithm>
#include <limits>

#include "GSMContext/GSMContext.h"
#include "esp_log.h"

AsyncGSM::AsyncGSM(GSMContext &context) {
  ctx = &context;
  ctx->transport().setDefaultSSL(isSecure());
}

AsyncGSM::AsyncGSM() {
  owns = true;
  ctx = new GSMContext();
  ctx->transport().setDefaultSSL(isSecure());
}

AsyncGSM::~AsyncGSM() {
  if (owns && ctx) {
    delete ctx;
    ctx = nullptr;
  }
  if (rxMutex) {
    vSemaphoreDelete(rxMutex);
    rxMutex = nullptr;
  }
}

RegStatus AsyncGSM::getRegistrationStatus() { return ctx->modem().URCState.creg.load(); }

int AsyncGSM::connect(IPAddress ip, uint16_t port) { return connect(ip.toString().c_str(), port); }

int AsyncGSM::connect(const char *host, uint16_t port) {
  log_i("Connecting to %s:%d", host, port);
  return modemConnect(host, port);
}

void AsyncGSM::stop() {
  modemStop();
  ctx->transport().reset();
  log_d("Connection stopped.");
}

size_t AsyncGSM::write(uint8_t c) { return write(&c, 1); }

size_t AsyncGSM::write(const uint8_t *buf, size_t size) {
  if (ctx->modem().URCState.isConnected.load() != ConnectionStatus::CONNECTED ||
      !ctx->at().getStream()) {
    log_e("Not connected or stream not initialized");
    return 0;
  }

  if (size == 0) return 0;
  log_d("Writing %zu bytes to modem...", size);

  String out;
  out.reserve(size);
  for (size_t i = 0; i < size; i++) {
    out += char(buf[i]);  // convert each byte to char
  }
  log_d("Sent bytes: %s", out.c_str());

  String command = isSecure() ? "AT+QSSLSEND=0," : "AT+QISEND=0,";

  ATPromise *promise = ctx->at().sendCommand(command + String(size));
  if (!promise) {
    log_e("Failed to create promise for AT+QISEND");
    ctx->at().popCompletedPromise(promise->getId());
    return 0;
  }

  while (!ctx->at().getStream()->available()) { vTaskDelay(0); }
  String response;
  while (ctx->at().getStream()->available()) {
    char c = ctx->at().getStream()->read();
    response += c;
    if (c == '>') { break; }
  }
  if (response.indexOf('>') == -1) {
    log_e("Did not receive prompt '>'");
    ctx->at().popCompletedPromise(promise->getId());
    return 0;
  }

  ctx->at().getStream()->write(buf, size);
  ctx->at().getStream()->flush();

  bool promptReceived = promise->expect("SEND OK")->wait();

  auto p = ctx->at().popCompletedPromise(promise->getId());
  if (!promptReceived) {
    log_e("Failed to get SEND OK confirmation");
    return 0;
  }

  log_d("Write successful.");
  return size;
}

int AsyncGSM::available() { return static_cast<int>(ctx->transport().available()); }

int AsyncGSM::read() {
  if (ctx->modem().URCState.isConnected.load() == ConnectionStatus::CLOSING && available() == 0) {
    ctx->modem().URCState.isConnected.store(ConnectionStatus::DISCONNECTED);
    return -1;
  }
  return ctx->transport().read();
}

int AsyncGSM::read(uint8_t *buf, size_t size) {
  if (ctx->modem().URCState.isConnected.load() == ConnectionStatus::CLOSING && available() == 0) {
    ctx->modem().URCState.isConnected.store(ConnectionStatus::DISCONNECTED);
    return 0;
  }
  return static_cast<int>(ctx->transport().read(buf, size));
}

int AsyncGSM::peek() { return ctx->transport().peek(); }

void AsyncGSM::flush() {
  log_w("Flushing stream...");
  if (!ctx->at().getStream()) {
    log_e("Stream not initialized");
    return;
  }
  ctx->transport().flush();
}

uint8_t AsyncGSM::connected() {
  auto status = ctx->modem().URCState.isConnected.load();
  return status == ConnectionStatus::CONNECTED || status == ConnectionStatus::CLOSING;
}
