#include "AsyncMqttGSM.h"
#include "esp_log.h"

AsyncMqttGSM::AsyncMqttGSM() {}

bool AsyncMqttGSM::init(AsyncEG915U &modem, AsyncATHandler &atHandler) {
  log_i("Initializing AsyncMqttGSM...");
  this->modem = &modem;
  this->at = &atHandler;
  this->modem->mqttQueueSub = &mqttQueueSub;

  ATPromise *mqttPromise = at->sendCommand("AT+QMTCFG=\"recv/mode\",0,1");
  if (!mqttPromise->wait()) {
    log_e("Failed to set Receive mode");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());

  mqttPromise = at->sendCommand("AT+QMTCFG=\"version\",0,4");
  if (!mqttPromise->wait()) {
    log_e("Failed to set MQTT version");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());

  // Set PDP context ID to 1
  mqttPromise = at->sendCommand("AT+QMTCFG=\"pdpcid\",0,1");
  if (!mqttPromise->wait()) {
    log_e("Failed to set PDP context ID");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());

  // Set keepalive to 60 seconds
  mqttPromise = at->sendCommand("AT+QMTCFG=\"keepalive\",0,120");
  if (!mqttPromise->wait()) {
    log_e("Failed to set keepalive");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());

  // Set clean session to 1 (true)
  mqttPromise = at->sendCommand("AT+QMTCFG=\"session\",0,0");
  if (!mqttPromise->wait()) {
    log_e("Failed to set session");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());

  // Set command timeout to 5 seconds, 3 retries, no exponential backoff
  mqttPromise = at->sendCommand("AT+QMTCFG=\"timeout\",0,5,3,0");
  if (!mqttPromise->wait()) {
    log_e("Failed to set timeout");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());
  return true;
}

AsyncMqttGSM &AsyncMqttGSM::setServer(const char *domain, uint16_t port) {
  this->domain = domain;
  this->port = port;
  return *this;
}

uint8_t AsyncMqttGSM::connected() {
  return modem->URCState.mqttState.load() == MqttConnectionState::CONNECTED;
}

bool AsyncMqttGSM::connect(const char *apn, const char *user,
                           const char *pass) {

  this->apn = apn;
  this->user = user;
  this->pass = pass;
  // restarts connection process
  modem->URCState.mqttState.store(MqttConnectionState::IDLE);

  ATPromise *mqttPromise = at->sendCommand("AT+QMTOPEN=0,\"" + String(domain) +
                                           "\"," + String(port));
  if (!mqttPromise->wait()) {
    log_e("Failed to open MQTT connection");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());

  // Wait on +QMTOPEN URC
  mqttPromise = at->sendCommand("");
  if (!mqttPromise->expect("+QMTOPEN: 0,0")->wait()) {
    log_e("Failed to get MQTT open URC");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());

  mqttPromise =
      at->sendCommand(String("AT+QMTCONN=0,\"") + apn + "\",\"" +
                      (user ? user : "") + "\",\"" + (pass ? pass : "") + "\"");

  if (!mqttPromise->wait()) {
    log_e("Failed to connect MQTT");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());

  mqttPromise = at->sendCommand("");
  if (!mqttPromise->expect("+QMTCONN: 0,0,0")->wait()) {
    log_e("Failed to get MQTT open URC");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());

  modem->URCState.mqttState.store(MqttConnectionState::CONNECTED);
  return true;
}

bool AsyncMqttGSM::publish(const char *topic, const uint8_t *payload,
                           unsigned int plength) {
  // Client: 0, msgId: 1, qos: 1, retain: 0
  String cmd =
      String("AT+QMTPUBEX=0,1,1,0,\"") + topic + "\"," + String(plength);
  ATPromise *mqttPromise = at->sendCommand(cmd);
  if (!mqttPromise->expect(">")->wait()) {
    log_e("Failed to publish MQTT topic");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());

  // Send payload
  String payloadStr;
  payloadStr.reserve(plength);
  for (unsigned int i = 0; i < plength; i++) {
    payloadStr += char(payload[i]);
  }
  mqttPromise = at->sendCommand(payloadStr);
  if (!mqttPromise->wait()) {
    log_e("Failed to publish MQTT payload");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());

  mqttPromise = at->sendCommand("");
  if (!mqttPromise->expect("+QMTPUBEX: 0")->wait() ||
      !mqttPromise->getResponse()->containsResponse("+QMTPUBEX: 0,1,0")) {
    log_e("Failed to get MQTT publish confirmation");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());

  return true;
}

bool AsyncMqttGSM::subscribe(const char *topic) { return subscribe(topic, 0); }

bool AsyncMqttGSM::subscribe(const char *topic, uint8_t qos) {
  subscribedTopics.insert(topic);
  // Client: 0, msgId: 1, topic, qos
  String cmd = String("AT+QMTSUB=0,1,\"") + topic + "\"," + String(qos);
  ATPromise *mqttPromise = at->sendCommand(cmd);
  if (!mqttPromise->wait()) {
    log_e("Failed to subscribe MQTT topic");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());

  mqttPromise = at->sendCommand("");
  if (!mqttPromise->expect("+QMTSUB: 0,1,0")->wait()) {
    log_e("Failed to get MQTT subscribe confirmation");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());
  return true;
}

bool AsyncMqttGSM::unsubscribe(const char *topic) {
  String cmd = String("AT+QMTUNSUB=0,1,\"") + topic + "\"";
  ATPromise *mqttPromise = at->sendCommand(cmd);
  if (!mqttPromise->wait()) {
    log_e("Failed to unsubscribe MQTT topic");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());

  mqttPromise = at->sendCommand("");
  if (!mqttPromise->expect("+QMTUNSUB: 0,1,0")->wait()) {
    log_e("Failed to get MQTT unsubscribe confirmation");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());
  return true;
}

AsyncMqttGSM &AsyncMqttGSM::setCallback(AsyncMqttGSMCallback callback) {
  mqttCallback = callback;
  return *this;
}

void AsyncMqttGSM::loop() {
  if (modem->URCState.mqttState.load() == MqttConnectionState::IDLE) {
    return;
  }

  if (!mqttCallback) {
    return;
  }

  if (modem->URCState.mqttState.load() == MqttConnectionState::DISCONNECTED) {
    if (!reconnect()) {
      log_e("Failed to reconnect to MQTT server");
      return;
    }
  }

  if (modem->mqttQueueSub->size() == 0) {
    return;
  }

  MqttMessage message;
  while (mqttQueueSub.pop(message)) {
    mqttCallback((char *)message.topic.c_str(), message.payload.data(),
                 message.length);
  }
}

bool AsyncMqttGSM::reconnect() {
  log_i("Reconnecting to MQTT server...");
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
  modem->URCState.mqttState.store(MqttConnectionState::CONNECTED);
  return true;
}
