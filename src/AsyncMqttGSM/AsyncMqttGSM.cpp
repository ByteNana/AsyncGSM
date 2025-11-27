#include "AsyncMqttGSM.h"

#include "esp_log.h"

AsyncMqttGSM::AsyncMqttGSM(GSMContext &context) {
  ctx = &context;
  ctx->modem().mqttQueueSub = &mqttQueueSub;
}

AsyncMqttGSM::AsyncMqttGSM() {
  owns = true;
  ctx = new GSMContext();
  ctx->modem().mqttQueueSub = &mqttQueueSub;
}

AsyncMqttGSM::~AsyncMqttGSM() {
  // Detach our queue from the shared modem to avoid dangling pointer
  if (ctx && ctx->modem().mqttQueueSub == &mqttQueueSub) { ctx->modem().mqttQueueSub = nullptr; }
  if (owns && ctx) {
    delete ctx;
    ctx = nullptr;
  }
}

bool AsyncMqttGSM::init() {
  log_d("Initializing AsyncMqttGSM...");

  if (!ctx->at().sendSync(String("AT+QMTCFG=\"recv/mode\",") + cidx + ",1")) {
    log_e("Failed to set Receive mode");
    return false;
  }

  if (!ctx->at().sendSync(String("AT+QMTCFG=\"version\",") + cidx + ",4")) {
    log_e("Failed to set MQTT version");
    return false;
  }

  // Set PDP context ID to 1
  if (!ctx->at().sendSync(String("AT+QMTCFG=\"pdpcid\",") + cidx)) {
    log_e("Failed to set PDP context ID");
    return false;
  }

  // Set keepalive to 60 seconds
  if (!ctx->at().sendSync(String("AT+QMTCFG=\"keepalive\",") + cidx + ",120")) {
    log_e("Failed to set keepalive");
    return false;
  }

  // Set clean session to 1 (true)
  if (!ctx->at().sendSync(String("AT+QMTCFG=\"session\",") + cidx + ",0")) {
    log_e("Failed to set session");
    return false;
  }

  // Set command timeout to 5 seconds, 3 retries, no exponential backoff
  if (!ctx->at().sendSync(String("AT+QMTCFG=\"timeout\",") + cidx + ",5,3,0")) {
    log_e("Failed to set timeout");
    return false;
  }
  return true;
}

AsyncMqttGSM &AsyncMqttGSM::setServer(const char *domain, uint16_t port) {
  this->domain = domain;
  this->port = port;
  return *this;
}

uint8_t AsyncMqttGSM::connected() {
  return ctx->modem().URCState.mqttState.load() == MqttConnectionState::CONNECTED;
}

bool AsyncMqttGSM::connect(const char *apn, const char *user, const char *pass) {
  this->apn = apn;
  this->user = user;
  this->pass = pass;

  // restarts connection process
  ctx->modem().URCState.mqttState.store(MqttConnectionState::IDLE);

  ATPromise *mqttPromise = ctx->at().sendCommand(
      String("AT+QMTOPEN=") + cidx + ",\"" + String(domain) + "\"," + String(port));
  if (!mqttPromise->wait()) {
    log_e("Failed to open MQTT connection");
    ctx->at().popCompletedPromise(mqttPromise->getId());
    return false;
  }
  ctx->at().popCompletedPromise(mqttPromise->getId());

  // Wait on +QMTOPEN URC
  mqttPromise = ctx->at().sendCommand("");
  if (!mqttPromise->expect(String("+QMTOPEN: ") + cidx + ",0")->wait()) {
    log_e("Failed to get MQTT open URC");
    ctx->at().popCompletedPromise(mqttPromise->getId());
    return false;
  }
  ctx->at().popCompletedPromise(mqttPromise->getId());

  String cmd = String("AT+QMTCONN=") + cidx + ",\"" + apn + "\"";
  if (user && strlen(user) > 0) { cmd += ",\"" + String(user) + "\""; }
  if (pass && strlen(pass) > 0) { cmd += ",\"" + String(pass) + "\""; }
  mqttPromise = ctx->at().sendCommand(cmd);

  if (!mqttPromise->wait()) {
    log_e("Failed to connect MQTT");
    ctx->at().popCompletedPromise(mqttPromise->getId());
    return false;
  }
  ctx->at().popCompletedPromise(mqttPromise->getId());

  mqttPromise = ctx->at().sendCommand("");
  if (!mqttPromise->expect(String("+QMTCONN: ") + cidx + ",0,0")->wait()) {
    log_e("Failed to get MQTT Connection URC");
    ctx->at().popCompletedPromise(mqttPromise->getId());
    return false;
  }
  ctx->at().popCompletedPromise(mqttPromise->getId());

  ctx->modem().URCState.mqttState.store(MqttConnectionState::CONNECTED);
  return true;
}

bool AsyncMqttGSM::publish(const char *topic, const uint8_t *payload, unsigned int plength) {
  // Client: 0, msgId: 1, qos: 1, retain: 0
  String cmd = String("AT+QMTPUBEX=") + cidx + ",1,1,0,\"" + topic + "\"," + String(plength);
  ATPromise *mqttPromise = ctx->at().sendCommand(cmd);
  if (!mqttPromise->expect(">")->wait()) {
    log_e("Failed to publish MQTT topic");
    ctx->at().popCompletedPromise(mqttPromise->getId());
    return false;
  }
  ctx->at().popCompletedPromise(mqttPromise->getId());

  // Send payload
  String payloadStr;
  payloadStr.reserve(plength);
  for (unsigned int i = 0; i < plength; i++) { payloadStr += char(payload[i]); }
  mqttPromise = ctx->at().sendCommand(payloadStr);
  if (!mqttPromise->wait()) {
    log_e("Failed to publish MQTT payload");
    ctx->at().popCompletedPromise(mqttPromise->getId());
    return false;
  }
  ctx->at().popCompletedPromise(mqttPromise->getId());

  mqttPromise = ctx->at().sendCommand("");
  if (!mqttPromise->expect(String("+QMTPUBEX: ") + cidx)->wait() ||
      !mqttPromise->getResponse()->containsResponse(String("+QMTPUBEX: ") + cidx + ",1,0")) {
    log_e("Failed to get MQTT publish confirmation");
    ctx->at().popCompletedPromise(mqttPromise->getId());
    return false;
  }
  ctx->at().popCompletedPromise(mqttPromise->getId());

  return true;
}

bool AsyncMqttGSM::subscribe(const char *topic) { return subscribe(topic, 0); }

bool AsyncMqttGSM::subscribe(const char *topic, uint8_t qos) {
  subscribedTopics.insert(topic);
  // Client: 0, msgId: 1, topic, qos
  String cmd = String("AT+QMTSUB=") + cidx + ",1,\"" + topic + "\"," + String(qos);
  ATPromise *mqttPromise = ctx->at().sendCommand(cmd);
  if (!mqttPromise->wait()) {
    log_e("Failed to subscribe MQTT topic");
    ctx->at().popCompletedPromise(mqttPromise->getId());
    return false;
  }
  ctx->at().popCompletedPromise(mqttPromise->getId());

  mqttPromise = ctx->at().sendCommand("");
  if (!mqttPromise->expect(String("+QMTSUB: ") + cidx + ",1,0")->wait()) {
    log_e("Failed to get MQTT subscribe confirmctx->ation");
    ctx->at().popCompletedPromise(mqttPromise->getId());
    return false;
  }
  ctx->at().popCompletedPromise(mqttPromise->getId());
  return true;
}

bool AsyncMqttGSM::unsubscribe(const char *topic) {
  String cmd = String("AT+QMTUNSUB=") + cidx + ",1,\"" + topic + "\"";
  ATPromise *mqttPromise = ctx->at().sendCommand(cmd);
  if (!mqttPromise->wait()) {
    log_e("Failed to unsubscribe MQTT topic");
    ctx->at().popCompletedPromise(mqttPromise->getId());
    return false;
  }
  ctx->at().popCompletedPromise(mqttPromise->getId());

  mqttPromise = ctx->at().sendCommand("");
  if (!mqttPromise->expect(String("+QMTUNSUB: ") + cidx + ",1,0")->wait()) {
    log_e("Failed to get MQTT unsubscribe confirmctx->ation");
    ctx->at().popCompletedPromise(mqttPromise->getId());
    return false;
  }
  ctx->at().popCompletedPromise(mqttPromise->getId());
  return true;
}

AsyncMqttGSM &AsyncMqttGSM::setCallback(AsyncMqttGSMCallback callback) {
  mqttCallback = callback;
  return *this;
}

void AsyncMqttGSM::loop() {
  if (ctx->modem().URCState.mqttState.load() == MqttConnectionState::IDLE) { return; }

  if (!mqttCallback) { return; }

  if (ctx->modem().URCState.mqttState.load() == MqttConnectionState::DISCONNECTED) {
    if (!reconnect()) {
      log_e("Failed to reconnect to MQTT server");
      return;
    }
  }

  if (!ctx->modem().mqttQueueSub) { return; }
  if (ctx->modem().mqttQueueSub->size() == 0) { return; }

  MqttMessage message;
  while (ctx->modem().mqttQueueSub->pop(message)) {
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
  ctx->modem().URCState.mqttState.store(MqttConnectionState::CONNECTED);
  return true;
}
