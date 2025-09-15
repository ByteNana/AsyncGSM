#include "MqttQueue.h"
#include "esp_log.h"
#include <memory>

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
  auto sp = new MqttMessagePtr(std::make_shared<MqttMessage>(msg));
  if (xQueueSend(messageQueue, &sp, timeout) == pdTRUE) {
    hasMessage.store(true);
    return true;
  }
  delete sp;
  log_w("Message queue full, dropping MQTT message");
  return false;
}

bool AtomicMqttQueue::pop(MqttMessage &msg) {
  MqttMessagePtr *sptr = nullptr;
  if (xQueueReceive(messageQueue, &sptr, 0) == pdTRUE && sptr) {
    if (*sptr) {
      msg = *(*sptr);
    }
    delete sptr;
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
