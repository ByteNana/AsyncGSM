#include "MqttQueue.h"
#include "esp_log.h"
#include <memory>

AtomicMqttQueue::AtomicMqttQueue() {
  messageQueue = xQueueCreate(10, sizeof(MqttMessage *));
  if (messageQueue == NULL) {
    log_e("Failed to create message queue - CRITICAL");
  }
}

AtomicMqttQueue::~AtomicMqttQueue() {
  if (messageQueue != NULL) {
    MqttMessage *raw = nullptr;
    while (xQueueReceive(messageQueue, &raw, 0) == pdTRUE) {
      std::unique_ptr<MqttMessage> uptr(raw);
    }
    hasMessage.store(false);
    vQueueDelete(messageQueue);
  }
}

bool AtomicMqttQueue::push(const MqttMessage &msg, TickType_t timeout) {
  auto ptr = std::make_unique<MqttMessage>(msg);
  MqttMessage *raw = ptr.get();
  if (xQueueSend(messageQueue, &raw, timeout) == pdTRUE) {
    auto a = ptr.release();
    hasMessage.store(true);
    return true;
  }
  log_w("Message queue full, dropping MQTT message");
  return false;
}

bool AtomicMqttQueue::pop(MqttMessage &msg) {
  MqttMessage *raw = nullptr;
  if (xQueueReceive(messageQueue, &raw, 0) == pdTRUE) {
    std::unique_ptr<MqttMessage> uptr(raw);
    msg = *uptr;
    if (uxQueueMessagesWaiting(messageQueue) == 0) {
      hasMessage.store(false);
    }
    return true;
  }
  return false;
}

bool AtomicMqttQueue::empty() { return !hasMessage.load(); }

size_t AtomicMqttQueue::size() { return uxQueueMessagesWaiting(messageQueue); }

void AtomicMqttQueue::clear() {
  if (messageQueue != NULL) {
    MqttMessage *raw = nullptr;
    while (xQueueReceive(messageQueue, &raw, 0) == pdTRUE) {
      std::unique_ptr<MqttMessage> uptr(raw);
    }
  }
  hasMessage.store(false);
}
