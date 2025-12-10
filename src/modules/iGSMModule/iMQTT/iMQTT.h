#pragma once

#include <Arduino.h>

#include <atomic>

#include "MqttQueue/MqttQueue.h"
#include "iMQTT.types.h"

class iMQTT {
 public:
  virtual ~iMQTT() = default;

  virtual bool begin() = 0;
  virtual AtomicMqttQueue *getQueue() = 0;
  virtual void setQueue(AtomicMqttQueue *queue) = 0;
  virtual MqttConnectionState getState() = 0;
  virtual void setState(MqttConnectionState state) = 0;

  // MQTT Operations
  virtual bool connect(
      const char *host, uint16_t port, const char *apn, const char *user = nullptr,
      const char *pass = nullptr) = 0;
  virtual bool disconnect() = 0;
  virtual bool publish(
      const char *topic, const uint8_t *payload, size_t length, uint8_t qos = 0,
      bool retain = false) = 0;
  virtual bool subscribe(const char *topic, uint8_t qos = 0) = 0;
  virtual bool unsubscribe(const char *topic) = 0;
  virtual void setSecurityLevel(bool secure, const char *ssl_cidx) = 0;

 protected:
  const char *cidx = "1";
  std::atomic<MqttConnectionState> mqttState{MqttConnectionState::IDLE};
  AtomicMqttQueue *mqttQueueSub = nullptr;
};
