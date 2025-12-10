#pragma once

#include <Arduino.h>
#include <AsyncATHandler.h>
#include <modules/iGSMModule/iMQTT/iMQTT.h>

class EG915MQTT : public iMQTT {
 public:
  EG915MQTT() = default;
  ~EG915MQTT() override = default;

  void init(AsyncATHandler &handler);
  bool begin() override;
  AtomicMqttQueue *getQueue() override;
  void setQueue(AtomicMqttQueue *queue) override;
  MqttConnectionState getState() override;
  void setState(MqttConnectionState state) override;

  bool connect(
      const char *host, uint16_t port, const char *apn, const char *user = nullptr,
      const char *pass = nullptr) override;
  bool disconnect() override;
  bool publish(
      const char *topic, const uint8_t *payload, size_t length, uint8_t qos = 0,
      bool retain = false) override;
  bool subscribe(const char *topic, uint8_t qos = 0) override;
  bool unsubscribe(const char *topic) override;
  void setSecurityLevel(bool secure, const char *ssl_cidx) override;

 private:
  AsyncATHandler *at = nullptr;
};