#include "GSMTransport.h"

#include <algorithm>

#include "freertos/FreeRTOS.h"

GSMTransport::GSMTransport() {}

void GSMTransport::init(Stream &s, SemaphoreHandle_t mutex) {
  stream = &s;
  rxMutex = mutex;
  reset();
}

void GSMTransport::reset() {
  lock();
  buffer.clear();
  pendingChannels.clear();
  awaitingChunk = false;
  autopollPending = false;
  lastChannel = defaultChannel;
  unlock();
}

void GSMTransport::setDefaultSSL(bool enabled) {
  lock();
  defaultChannel = enabled ? Channel::SSL : Channel::TCP;
  lastChannel = defaultChannel;
  unlock();
}

void GSMTransport::lock() {
  if (rxMutex) {
    xSemaphoreTake(rxMutex, portMAX_DELAY);
  }
}

void GSMTransport::unlock() {
  if (rxMutex) {
    xSemaphoreGive(rxMutex);
  }
}

void GSMTransport::requestChannel(Channel ch) {
  if (!stream)
    return;
  lastChannel = ch;
  if (ch == Channel::SSL) {
    log_d("GSMTransport requesting SSL chunk");
    stream->print("AT+QSSLRECV=0,1500\r\n");
  } else {
    log_d("GSMTransport requesting TCP chunk");
    stream->print("AT+QIRD=0\r\n");
  }
  stream->flush();
}

void GSMTransport::maybeRequestNext() {
  Channel nextChannel = defaultChannel;
  bool shouldRequest = false;

  lock();
  if (!awaitingChunk && buffer.empty()) {
    if (!pendingChannels.empty()) {
      nextChannel = pendingChannels.front();
      pendingChannels.pop_front();
      awaitingChunk = true;
      shouldRequest = true;
      autopollPending = false;
    } else if (autopollPending) {
      awaitingChunk = true;
      shouldRequest = true;
      autopollPending = false;
      nextChannel = lastChannel;
    }
  }
  unlock();

  if (shouldRequest) {
    requestChannel(nextChannel);
  }
}

void GSMTransport::notifyDataReady(bool isSSL) {
  Channel ch = isSSL ? Channel::SSL : Channel::TCP;
  lock();
  pendingChannels.push_back(ch);
  unlock();
}

void GSMTransport::deliverChunk(std::vector<uint8_t> &&chunk) {
  Channel nextChannel = defaultChannel;
  bool shouldRequest = false;
  bool enableAutopoll = false;

  lock();
  awaitingChunk = false;
  if (!chunk.empty()) {
    buffer.insert(buffer.end(), chunk.begin(), chunk.end());
  }
  if (chunk.size() >= MAX_CHUNK_SIZE) {
    enableAutopoll = true;
  }
  if (buffer.empty()) {
    if (!pendingChannels.empty()) {
      nextChannel = pendingChannels.front();
      pendingChannels.pop_front();
      awaitingChunk = true;
      shouldRequest = true;
      autopollPending = false;
    } else if (enableAutopoll) {
      awaitingChunk = true;
      shouldRequest = true;
      autopollPending = false;
      nextChannel = lastChannel;
    }
  } else if (enableAutopoll) {
    autopollPending = true;
  }
  unlock();

  if (shouldRequest) {
    requestChannel(nextChannel);
  }
}

size_t GSMTransport::available() {
  maybeRequestNext();
  lock();
  size_t count = buffer.size();
  unlock();
  return count;
}

int GSMTransport::read() {
  uint8_t byte{0};
  size_t copied = read(&byte, 1);
  if (copied == 0)
    return -1;
  return static_cast<int>(byte);
}

size_t GSMTransport::read(uint8_t *buf, size_t size) {
  if (!buf || size == 0)
    return 0;

  lock();
  size_t toCopy = std::min<size_t>(size, buffer.size());
  for (size_t i = 0; i < toCopy; ++i) {
    buf[i] = buffer.front();
    buffer.pop_front();
  }
  bool needRequest = false;
  bool autopollNeeded = false;
  if (buffer.empty() && !awaitingChunk) {
    needRequest = !pendingChannels.empty();
    autopollNeeded = autopollPending;
  }
  unlock();

  if (needRequest || autopollNeeded) {
    maybeRequestNext();
  }

  return toCopy;
}

int GSMTransport::peek() {
  lock();
  if (buffer.empty()) {
    bool trigger = !awaitingChunk && !pendingChannels.empty();
    unlock();
    if (trigger) {
      maybeRequestNext();
      vTaskDelay(pdMS_TO_TICKS(1));
      lock();
    } else {
      return -1;
    }
  }
  if (buffer.empty()) {
    unlock();
    return -1;
  }
  int value = buffer.front();
  unlock();
  return value;
}

void GSMTransport::flush() {
  if (stream)
    stream->flush();
}
