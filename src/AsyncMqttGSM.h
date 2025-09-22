#pragma once

#include "AsyncGuard.h"
#include "EG915/EG915.h"
#include <set>

using AsyncMqttGSMCallback =
    std::function<void(char *, uint8_t *, unsigned int)>;

class AsyncMqttGSM {
private:
  AsyncEG915U *modem = nullptr;
  AsyncATHandler *at = nullptr;
  AtomicMqttQueue mqttQueueSub;

  AsyncMqttGSMCallback mqttCallback = nullptr;

  const char *domain;
  uint16_t port;
  const char *apn;
  const char *user;
  const char *pass;
  std::set<const char *> subscribedTopics;

  bool reconnect();

public:
  AsyncMqttGSM();
  ~AsyncMqttGSM() = default;

  bool init(AsyncEG915U &modem, AsyncATHandler &atHandler);

  AsyncMqttGSM &setServer(const char *domain, uint16_t port);
  uint8_t connected();
  bool connect(const char *id, const char *user, const char *pass);
  bool publish(const char *topic, const uint8_t *payload, unsigned int plength);
  bool subscribe(const char *topic);
  bool subscribe(const char *topic, uint8_t qos);
  bool unsubscribe(const char *topic);
  AsyncMqttGSM &setCallback(AsyncMqttGSMCallback callback);
  void loop();
};
