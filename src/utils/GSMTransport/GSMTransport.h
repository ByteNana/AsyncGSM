#pragma once

#include <Arduino.h>
#include <Stream.h>

#include <deque>
#include <vector>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"

class GSMTransport {
 public:
  GSMTransport();

  void init(Stream &stream, SemaphoreHandle_t mutex);
  void reset();
  void setDefaultSSL(bool enabled);

  void notifyDataReady(bool isSSL);
  void deliverChunk(std::vector<uint8_t> &&chunk);

  size_t available();
  int read();
  size_t read(uint8_t *buf, size_t size);
  int peek();
  void flush();

 private:
  enum class Channel { TCP, SSL };

  void lock();
  void unlock();
  void maybeRequestNext();
  void requestChannel(Channel ch);

  Stream *stream{nullptr};
  SemaphoreHandle_t rxMutex{nullptr};
  std::deque<uint8_t> buffer;
  std::deque<Channel> pendingChannels;
  bool awaitingChunk{false};
  Channel defaultChannel{Channel::TCP};
  Channel lastChannel{Channel::TCP};
  bool autopollPending{false};

  static constexpr size_t MAX_CHUNK_SIZE = 1500;
};
