#include "MQTT.h"

#include <esp_log.h>

void EG915MQTT::init(AsyncATHandler &handler) { at = &handler; }

bool EG915MQTT::begin() {
  if (!at) {
    log_e("AT handler not initialized");
    return false;
  }

  log_d("Initializing AsyncMqttGSM...");

  if (!at->sendSync(String("AT+QMTCFG=\"recv/mode\",") + cidx + ",1")) {
    log_e("Failed to set Receive mode");
    return false;
  }

  if (!at->sendSync(String("AT+QMTCFG=\"version\",") + cidx + ",4")) {
    log_e("Failed to set MQTT version");
    return false;
  }

  // Set PDP context ID to 1
  if (!at->sendSync(String("AT+QMTCFG=\"pdpcid\",") + cidx)) {
    log_e("Failed to set PDP context ID");
    return false;
  }

  // Set keepalive to 60 seconds
  if (!at->sendSync(String("AT+QMTCFG=\"keepalive\",") + cidx + ",120")) {
    log_e("Failed to set keepalive");
    return false;
  }

  // Set clean session to 1 (true)
  if (!at->sendSync(String("AT+QMTCFG=\"session\",") + cidx + ",0")) {
    log_e("Failed to set session");
    return false;
  }

  // Set command timeout to 5 seconds, 3 retries, no exponential backoff
  if (!at->sendSync(String("AT+QMTCFG=\"timeout\",") + cidx + ",5,3,0")) {
    log_e("Failed to set timeout");
    return false;
  }
  return true;
}

AtomicMqttQueue *EG915MQTT::getQueue() { return mqttQueueSub; }

void EG915MQTT::setQueue(AtomicMqttQueue *queue) { mqttQueueSub = queue; }

MqttConnectionState EG915MQTT::getState() { return mqttState.load(); }

void EG915MQTT::setState(MqttConnectionState state) { mqttState.store(state); }

bool EG915MQTT::connect(
    const char *host, uint16_t port, const char *apn, const char *user, const char *pass) {
  if (!at) return false;
  const char *cidx = "1";

  // Reset state
  setState(MqttConnectionState::IDLE);

  ATPromise *mqttPromise =
      at->sendCommand(String("AT+QMTOPEN=") + cidx + ",\"" + String(host) + ",\"" + String(port));
  if (!mqttPromise->wait()) {
    log_e("Failed to open MQTT connection");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());

  // Wait on +QMTOPEN URC
  mqttPromise = at->sendCommand("");
  if (!mqttPromise->expect(String("+QMTOPEN: ") + cidx + ",0")->wait()) {
    log_e("Failed to get MQTT open URC");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());

  String cmd = String("AT+QMTCONN=") + cidx + ",\"" + apn + "\"";
  if (user && strlen(user) > 0) { cmd += ",\"" + String(user) + "\""; }
  if (pass && strlen(pass) > 0) { cmd += ",\"" + String(pass) + "\""; }
  mqttPromise = at->sendCommand(cmd);

  if (!mqttPromise->wait()) {
    log_e("Failed to connect MQTT");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());

  mqttPromise = at->sendCommand("");
  if (!mqttPromise->expect(String("+QMTCONN: ") + cidx + ",0,0")->wait()) {
    log_e("Failed to get MQTT Connection URC");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());

  setState(MqttConnectionState::CONNECTED);
  return true;
}

bool EG915MQTT::disconnect() {
  if (!at) return false;
  const char *cidx = "1";
  bool ret = at->sendSync(String("AT+QMTDISC=") + cidx);
  if (ret) setState(MqttConnectionState::DISCONNECTED);
  return ret;
}

bool EG915MQTT::publish(
    const char *topic, const uint8_t *payload, size_t length, uint8_t qos, bool retain) {
  if (!at) return false;

  // AT+QMTPUBEX=<tcpconnectID>,<msgID>,<qos>,<retain>,"<topic>",<length>
  // msgID we use 1 for simplicity for now
  String cmd = String("AT+QMTPUBEX=") + cidx + ",1," + String(qos) + "," + (retain ? "1" : "0") +
               ",\"" + topic + ",\"" + String(length);

  ATPromise *mqttPromise = at->sendCommand(cmd);
  if (!mqttPromise->expect(">")->wait()) {
    log_e("Failed to publish MQTT topic");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());

  // Send payload
  // We should write binary data
  at->getStream()->write(payload, length);
  at->getStream()->flush();

  // Wait for confirmation
  mqttPromise = at->sendCommand("");
  // +QMTPUBEX: <tcpconnectID>,<msgID>,<result>
  // result 0 = success
  if (!mqttPromise->expect(String("+QMTPUBEX: ") + cidx)->wait() ||
      !mqttPromise->getResponse()->containsResponse(String("+QMTPUBEX: ") + cidx + ",1,0")) {
    log_e("Failed to get MQTT publish confirmation");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());

  return true;
}

bool EG915MQTT::subscribe(const char *topic, uint8_t qos) {
  if (!at) return false;

  // AT+QMTSUB=<tcpconnectID>,<msgID>,"<topic>",<qos>
  String cmd = String("AT+QMTSUB=") + cidx + ",1,\"" + topic + ",\"" + String(qos);
  ATPromise *mqttPromise = at->sendCommand(cmd);
  if (!mqttPromise->wait()) {
    log_e("Failed to subscribe MQTT topic");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());

  // Wait for URC +QMTSUB: <tcpconnectID>,<msgID>,<result>
  mqttPromise = at->sendCommand("");
  if (!mqttPromise->expect(String("+QMTSUB: ") + cidx + ",1,0")->wait()) {
    log_e("Failed to get MQTT subscribe confirmation");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());
  return true;
}

bool EG915MQTT::unsubscribe(const char *topic) {
  if (!at) return false;

  // AT+QMTUNSUB=<tcpconnectID>,<msgID>,"<topic>"
  String cmd = String("AT+QMTUNSUB=") + cidx + ",1,\"" + topic + "\"";
  ATPromise *mqttPromise = at->sendCommand(cmd);
  if (!mqttPromise->wait()) {
    log_e("Failed to unsubscribe MQTT topic");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());

  // Wait for URC +QMTUNSUB: <tcpconnectID>,<msgID>,<result>
  mqttPromise = at->sendCommand("");
  if (!mqttPromise->expect(String("+QMTUNSUB: ") + cidx + ",1,0")->wait()) {
    log_e("Failed to get MQTT unsubscribe confirmation");
    at->popCompletedPromise(mqttPromise->getId());
    return false;
  }
  at->popCompletedPromise(mqttPromise->getId());
  return true;
}

void EG915MQTT::setSecurityLevel(bool secure, const char *ssl_cidx) {
  if (!at) return;

  String secureLevel = (secure ? "1" : "0");
  // Enable SSL for the MQTT client and bind to SSL context index
  if (!at->sendSync(String("AT+QMTCFG=\"ssl\",") + cidx + "," + secureLevel + "," + ssl_cidx)) {
    log_e("Failed to set SSL for MQTT");
  }
}