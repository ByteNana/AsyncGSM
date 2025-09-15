#pragma once

#include "freertos/FreeRTOS.h"
#include <Arduino.h>
#include <atomic>
#include <queue>
#include <memory>

struct MqttMessage {
  String topic;
  std::vector<uint8_t> payload;
  unsigned int length;

  MqttMessage() : length(0) {}
  MqttMessage(const String &t, const std::vector<uint8_t> &p, unsigned int l)
      : topic(t), payload(p), length(l) {}
};

using MqttMessagePtr = std::shared_ptr<MqttMessage>;

class AtomicMqttQueue {
private:
  QueueHandle_t messageQueue;
  std::atomic<bool> hasMessage{false};

public:
  AtomicMqttQueue();

  ~AtomicMqttQueue();
  bool push(const MqttMessage &msg, TickType_t timeout = 0);
  bool pop(MqttMessage &msg);
  bool empty();
  size_t size();
  void clear();
};
