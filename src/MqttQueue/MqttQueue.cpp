#include "MqttQueue.h"
#include "esp_log.h"

AtomicMqttQueue::AtomicMqttQueue() {
  messageQueue = xQueueCreate(10, sizeof(MqttMessage));
  if (messageQueue == NULL) {
    log_e("Failed to create message queue - CRITICAL");
  }
}

AtomicMqttQueue::~AtomicMqttQueue() {
  if (messageQueue != NULL) {
    vQueueDelete(messageQueue);
  }
}

bool AtomicMqttQueue::push(const MqttMessage &msg, TickType_t timeout) {
  if (xQueueSend(messageQueue, &msg, timeout) == pdTRUE) {
    hasMessage.store(true);
    return true;
  }
  log_w("Message queue full, dropping MQTT message");
  return false;
}

bool AtomicMqttQueue::pop(MqttMessage &msg) {
  if (xQueueReceive(messageQueue, &msg, 0) == pdTRUE) {
    if (uxQueueMessagesWaiting(messageQueue) == 0) {
      hasMessage.store(false);
    }
    return true;
  }
  return false;
}

bool AtomicMqttQueue::empty() {
  return !hasMessage.load();
}

size_t AtomicMqttQueue::size() {
  return uxQueueMessagesWaiting(messageQueue);
}

void AtomicMqttQueue::clear() {
  MqttMessage temp;
  while (pop(temp)) {
  }
  hasMessage.store(false);
}

