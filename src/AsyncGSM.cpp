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

int AsyncGSM::connect(IPAddress ip, uint16_t port) { return connect(ip.toString().c_str(), port); }

int AsyncGSM::connect(const char *host, uint16_t port) {
  if (!ctx->hasModule()) return 0;
  log_i("Connecting to %s:%d", host, port);
  return isSecure() ? ctx->modem().socket().connectSecure(host, port)
                    : ctx->modem().socket().connect(host, port);
}

void AsyncGSM::stop() {
  if (!ctx->hasModule()) return;
  log_v("Stopping connection...");
  isSecure() ? ctx->modem().socket().stopSecure() : ctx->modem().socket().stop();
  ctx->transport().reset();
  log_d("Connection stopped.");
}

size_t AsyncGSM::write(uint8_t c) { return write(&c, 1); }

size_t AsyncGSM::write(const uint8_t *buf, size_t size) {
  if (!ctx->hasModule() || ctx->modem().getConnectionStatus() != ConnectionStatus::CONNECTED) {
    log_e("Not connected");
    return 0;
  }
  return ctx->modem().socket().sendData(buf, size, isSecure());
}

int AsyncGSM::available() { return static_cast<int>(ctx->transport().available()); }

int AsyncGSM::read() {
  if (!ctx->hasModule()) return -1;
  if (ctx->modem().getConnectionStatus() == ConnectionStatus::CLOSING && available() == 0) {
    return -1;
  }
  return ctx->transport().read();
}

int AsyncGSM::read(uint8_t *buf, size_t size) {
  if (!ctx->hasModule()) return 0;
  if (ctx->modem().getConnectionStatus() == ConnectionStatus::CLOSING && available() == 0) {
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
  if (!ctx->hasModule()) return 0;
  auto status = ctx->modem().getConnectionStatus();
  return status == ConnectionStatus::CONNECTED || status == ConnectionStatus::CLOSING;
}