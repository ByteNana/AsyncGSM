#include "AsyncGSM.h"
#include "esp_log.h"

bool AsyncGSM::init(Stream &stream) {
  _connected = false;

  if (!at.begin(stream)) {
    log_e("Failed to initialize AsyncATHandler");
    return false;
  }
  return true;
}

int AsyncGSM::connect(IPAddress ip, uint16_t port) {
  return connect(ip.toString().c_str(), port);
}

int AsyncGSM::connect(const char *host, uint16_t port) {
  stop();
  at.flushResponseQueue();
  _connected = modemConnect(host, port);
  return _connected;
}

void AsyncGSM::stop() {
  at.flushResponseQueue();
  _connected = false;
  if (!at.sendCommand("AT+QIDEACT=1", "OK", 4000)) {
    log_e("Failed to deactivate GPRS context");
  }
}

size_t AsyncGSM::write(uint8_t c) { return write(&c, 1); }

size_t AsyncGSM::write(const uint8_t *buf, size_t size) {
  if (!at._stream) {
    log_e("Stream not initialized");
    return 0;
  }
  return at._stream->write(buf, size);
}

int AsyncGSM::available() {
  if (!at._stream) {
    log_e("Stream not initialized");
    return 0;
  }
  return at._stream->available();
}

int AsyncGSM::read() {
  if (!at._stream) {
    log_e("Stream not initialized");
    return -1;
  }
  return at._stream->read();
}

int AsyncGSM::read(uint8_t *buf, size_t size) {
  if (!at._stream) {
    log_e("Stream not initialized");
    return 0;
  }
  size_t bytesRead = 0;
  while (bytesRead < size && at._stream->available()) {
    int c = at._stream->read();
    if (c < 0) break;  // No more data available
    buf[bytesRead++] = static_cast<uint8_t>(c);
  }
  return bytesRead;
}

int AsyncGSM::peek() {
  if (!at._stream) {
    log_e("Stream not initialized");
    return -1;
  }
  return at._stream->peek();
}

void AsyncGSM::flush() {
  if (!at._stream) {
    log_e("Stream not initialized");
    return;
  }
  at._stream->flush();
}

uint8_t AsyncGSM::connected() {
  return _connected;
}
