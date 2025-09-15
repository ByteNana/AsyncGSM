#include "MqttQueue.h"
#include "esp_log.h"
#include <memory>

using MqttMessagePtr = std::shared_ptr<MqttMessage>;

AtomicMqttQueue::AtomicMqttQueue() {
  messageQueue = xQueueCreate(10, sizeof(MqttMessagePtr));
  if (messageQueue == NULL) {
    log_e("Failed to create message queue - CRITICAL");
  }
}

AtomicMqttQueue::~AtomicMqttQueue() {
  if (messageQueue != NULL) {
    MqttMessagePtr msg;
    while (xQueueReceive(messageQueue, &msg, 0) == pdTRUE) {
    }
    hasMessage.store(false);
    vQueueDelete(messageQueue);
  }
}

bool AtomicMqttQueue::push(const MqttMessage &msg, TickType_t timeout) {
  MqttMessagePtr ptr = std::make_shared<MqttMessage>(msg);
  if (xQueueSend(messageQueue, &ptr, timeout) == pdTRUE) {
    hasMessage.store(true);
    return true;
  }
  log_w("Message queue full, dropping MQTT message");
  return false;
}

bool AtomicMqttQueue::pop(MqttMessage &msg) {
  MqttMessagePtr ptr;
  if (xQueueReceive(messageQueue, &ptr, 0) == pdTRUE && ptr) {
    msg = *ptr;
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
    MqttMessagePtr ptr;
    while (xQueueReceive(messageQueue, &ptr, 0) == pdTRUE) {
    }
  }
  hasMessage.store(false);
}
