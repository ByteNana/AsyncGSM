#include "AsyncMqttGSM.h"

#include "esp_log.h"

AsyncMqttGSM::AsyncMqttGSM(GSMContext &context) {
  ctx = &context;
  if (ctx->hasModule()) { ctx->modem().mqtt().setQueue(&mqttQueueSub); }
}

AsyncMqttGSM::AsyncMqttGSM() {
  owns = true;
  ctx = new GSMContext();
  // ctx has no module yet
}

AsyncMqttGSM::~AsyncMqttGSM() {
  // Detach our queue from the shared modem to avoid dangling pointer
  if (ctx && ctx->hasModule() && ctx->modem().mqtt().getQueue() == &mqttQueueSub) {
    ctx->modem().mqtt().setQueue(nullptr);
  }
  if (owns && ctx) {
    delete ctx;
    ctx = nullptr;
  }
}

bool AsyncMqttGSM::init() {
  log_d("Initializing AsyncMqttGSM...");
  if (!ctx->hasModule()) {
    log_e("Modem not initialized");
    return false;
  }
  // Ensure queue is attached (in case constructor ran before module was attached)
  ctx->modem().mqtt().setQueue(&mqttQueueSub);

  return ctx->modem().mqtt().begin();
}

AsyncMqttGSM &AsyncMqttGSM::setServer(const char *domain, uint16_t port) {
  this->domain = domain;
  this->port = port;
  return *this;
}

uint8_t AsyncMqttGSM::connected() {
  if (!ctx->hasModule()) return 0;
  return ctx->modem().mqtt().getState() == MqttConnectionState::CONNECTED;
}

bool AsyncMqttGSM::connect(const char *apn, const char *user, const char *pass) {
  this->apn = apn;
  this->user = user;
  this->pass = pass;

  if (!ctx->hasModule()) return false;
  // Ensure queue is attached
  ctx->modem().mqtt().setQueue(&mqttQueueSub);

  return ctx->modem().mqtt().connect(domain, port, apn, user, pass);
}

bool AsyncMqttGSM::publish(const char *topic, const uint8_t *payload, unsigned int plength) {
  if (!ctx->hasModule()) return false;
  return ctx->modem().mqtt().publish(topic, payload, plength, 0, false);
}

bool AsyncMqttGSM::subscribe(const char *topic) { return subscribe(topic, 0); }

bool AsyncMqttGSM::subscribe(const char *topic, uint8_t qos) {
  subscribedTopics.insert(topic);
  if (!ctx->hasModule()) return false;
  return ctx->modem().mqtt().subscribe(topic, qos);
}

bool AsyncMqttGSM::unsubscribe(const char *topic) {
  subscribedTopics.erase(topic);
  if (!ctx->hasModule()) return false;
  return ctx->modem().mqtt().unsubscribe(topic);
}

AsyncMqttGSM &AsyncMqttGSM::setCallback(AsyncMqttGSMCallback callback) {
  mqttCallback = callback;
  return *this;
}

void AsyncMqttGSM::loop() {
  if (!ctx->hasModule()) return;
  if (ctx->modem().mqtt().getState() == MqttConnectionState::IDLE) { return; }

  if (!mqttCallback) { return; }

  if (ctx->modem().mqtt().getState() == MqttConnectionState::DISCONNECTED) {
    if (!reconnect()) {
      log_e("Failed to reconnect to MQTT server");
      return;
    }
  }

  AtomicMqttQueue *queue = ctx->modem().mqtt().getQueue();
  if (!queue) { return; }
  if (queue->size() == 0) { return; }

  MqttMessage message;
  while (queue->pop(message)) {
    mqttCallback((char *)message.topic.c_str(), message.payload.data(), message.length);
  }
}

bool AsyncMqttGSM::reconnect() {
  log_w("Reconnecting to MQTT server...");
  if (!connect(apn, user, pass)) {
    log_e("Failed to reconnect to MQTT server");
    return false;
  }

  // Resubscribe to topics
  for (const auto &topic : subscribedTopics) {
    if (!subscribe(topic)) {
      log_e("Failed to resubscribe to topic: %s", topic);
      return false;
    }
  }

  log_i("Reconnected to MQTT server and resubscribed to topics.");
  // State is handled by connect()
  return true;
}
