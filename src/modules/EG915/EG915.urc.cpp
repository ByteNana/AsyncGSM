#include "EG915.h"

#include <utils/GSMTransport/GSMTransport.h>
#include <utils/MqttQueue/MqttQueue.h>

#include <iomanip>
#include <utility>
#include <vector>

#include "esp_log.h"

static bool consumeOkResponse(Stream *stream) {
  String tail = "";
  while (stream->available()) {
    int c = stream->read();
    if (c >= 0) {
      tail += (char)c;
      if (tail.endsWith("OK\r\n")) { return true; }
      // Prevent tail string from growing without bound
      if (tail.length() > 8) { tail.remove(0, tail.length() - 8); }
    }
  }
  return false;
}

void AsyncEG915U::registerURCs() {
  if (!at) return;
  // Helper to register and remember pattern
  auto reg = [&](const String &pattern, std::function<void(const String &)> cb) {
    at->urc.registerEvent(pattern, cb);
    registeredURCPatterns.push_back(pattern);
  };

  // Network registration
  reg("+CREG:", [this](const String &u) { onRegChanged(u); });
  reg("+CGREG:", [this](const String &u) { onRegChanged(u); });
  reg("+CEREG:", [this](const String &u) { onRegChanged(u); });

  // TCP/SSL open results
  reg("+QIOPEN:", [this](const String &u) { onOpenResult(u); });
  reg("+QSSLOPEN:", [this](const String &u) { onOpenResult(u); });

  // Close notifications
  reg("+QICLOSE:", [this](const String &u) { onClosed(u); });
  reg("+QIURC: \"closed\"", [this](const String &u) { onClosed(u); });
  reg("+QSSLURC: \"closed\"", [this](const String &u) { onClosed(u); });

  // Data-ready notifications
  reg("+QIURC: \"recv\"", [this](const String &u) { onTcpRecv(u); });
  reg("+QSSLURC: \"recv\"", [this](const String &u) { onSslRecv(u); });

  // Read data notifications
  reg("+QIRD:", [this](const String &u) { onReadData(u); });
  reg("+QSSLRECV:", [this](const String &u) { onReadData(u); });

  // MQTT
  reg("+QMTRECV:", [this](const String &u) { onMqttRecv(u); });
  reg("+QMTSTAT:", [this](const String &u) { onMqttStat(u); });
}

void AsyncEG915U::unregisterURCs() {
  if (!at) return;
  for (const auto &p : registeredURCPatterns) { at->urc.unregisterEvent(p); }
  registeredURCPatterns.clear();
}

void AsyncEG915U::onRegChanged(const String &urc) {
  String trimmed = urc;
  trimmed.trim();
  int commaIndex = trimmed.indexOf(',');
  if (commaIndex != -1) {
    String statusStr = trimmed.substring(commaIndex + 1);
    int endPos = statusStr.indexOf(',');
    if (endPos != -1) statusStr = statusStr.substring(0, endPos);
    URCState.creg.store((RegStatus)statusStr.toInt());
    log_v("URC: Registration status updated to %d", URCState.creg.load());
  }
}

void AsyncEG915U::onOpenResult(const String &urc) {
  String trimmed = urc;
  trimmed.trim();
  if (transport) { transport->reset(); }
  int firstComma = trimmed.indexOf(',');
  if (firstComma != -1) {
    String resultStr = trimmed.substring(firstComma + 1);
    int endPos = resultStr.indexOf(',');
    if (endPos != -1) resultStr = resultStr.substring(0, endPos);
    int result = resultStr.toInt();
    if (result == 0) {
      URCState.isConnected.store(ConnectionStatus::CONNECTED);
      log_d("URC: Connection opened successfully");
    } else {
      URCState.isConnected.store(ConnectionStatus::FAILED);
      log_e("URC: Connection failed with error %d", result);
    }
  }
}

void AsyncEG915U::onClosed(const String & /*urc*/) {
  URCState.isConnected.store(ConnectionStatus::DISCONNECTED);
  if (transport) { transport->reset(); }
  log_d("URC: Connection closed");
}

void AsyncEG915U::onTcpRecv(const String & /*urc*/) {
  log_d("URC: Data received, ready to read with +QIRD");
  if (transport) { transport->notifyDataReady(false); }
}

void AsyncEG915U::onSslRecv(const String & /*urc*/) {
  log_d("URC: SSL Data received, ready to read with +QSSLRECV");
  if (transport) { transport->notifyDataReady(true); }
}

void AsyncEG915U::onReadData(const String &urc) {
  int headerStart = urc.indexOf(':');
  if (headerStart == -1) {
    log_e(">>>>>>>>>>>>URC: Failed to parse data length from +QIRD/+QSSLRECV");
    if (transport) { transport->deliverChunk(std::vector<uint8_t>()); }
    return;
  }
  String header = urc.substring(headerStart + 1);
  int remaining = header.toInt();
  if (remaining < 0) {
    log_e(">>>>>>>>>>>QIRD: Invalid length");
    if (transport) { transport->deliverChunk(std::vector<uint8_t>()); }
    return;
  }
  log_d("QIRD/QSSLRECV: Data length = %d", remaining);
  std::vector<uint8_t> chunk;
  chunk.reserve(remaining);
  unsigned long startTime = millis();
  const unsigned long timeout = 5000;
  while (remaining > 0) {
    if (millis() - startTime > timeout) {
      log_e("Timeout reading data from modem");
      break;
    }
    if (!at->getStream()->available()) {
      vTaskDelay(1);
      continue;
    }
    int c = at->getStream()->read();
    if (c < 0) {
      vTaskDelay(1);
      continue;
    }
    chunk.push_back(static_cast<uint8_t>(c));
    remaining--;
  }
  consumeOkResponse(at->getStream());
  log_v("Chunk: %.*s", chunk.size(), (char*)chunk.data());
  if (transport) { transport->deliverChunk(std::move(chunk)); }
}

void AsyncEG915U::onMqttRecv(const String &urc) {
  log_w("URC: +QMTRECV received");
  int commaCount = std::count(urc.begin(), urc.end(), ',');
  int headerStart = urc.indexOf(':');
  int firstComma = urc.indexOf(',');
  String afterFirstComma = urc.substring(firstComma + 1);
  afterFirstComma.trim();
  if (headerStart == -1 || firstComma == -1 || afterFirstComma.length() == 0) {
    log_e("URC: Failed to parse +QMTRECV header");
    return;
  }

  String clientId = urc.substring(headerStart + 1, firstComma);
  clientId.trim();
  int numEnd = 0;
  while (numEnd < afterFirstComma.length() && isDigit(afterFirstComma.charAt(numEnd))) { numEnd++; }
  String msgId = afterFirstComma.substring(0, numEnd);
  msgId.trim();
  log_d("URC: MQTT message for client %d on topic ID %d", clientId.toInt(), msgId.toInt());
  if (commaCount <= 1) {
    // QMTRECV=<client>,<msgid>
    at->getStream()->print("AT+QMTRECV=" + clientId + "," + msgId + "\r\n");
    at->getStream()->flush();
    return;
  }

  if (!mqttQueueSub) {
    log_w("URC: MQTT message received but mqttQueueSub is not set");
    return;
  }

  // Parse topic and payload when included in URC
  int secondComma = urc.indexOf(',', firstComma + 1);
  if (secondComma == -1) {
    log_e("URC: Failed to find topic");
    return;
  }
  int topicStart = urc.indexOf('"', secondComma + 1);
  if (topicStart == -1) {
    log_e("URC: Failed to find topic start quote");
    return;
  }
  int topicEnd = urc.indexOf('"', topicStart + 1);
  if (topicEnd == -1) {
    log_e("URC: Failed to find topic end quote");
    return;
  }
  String topic = urc.substring(topicStart + 1, topicEnd);
  int payloadStart = urc.indexOf('"', topicEnd + 1);
  if (payloadStart == -1) {
    log_e("URC: Failed to find payload start quote");
    return;
  }
  int payloadEnd = urc.lastIndexOf('"');
  if (payloadEnd <= payloadStart) {
    log_e("URC: Failed to find payload end quote");
    return;
  }
  String payload = urc.substring(payloadStart + 1, payloadEnd);
  log_w("URC: Topic: '%s', Payload: '%s'", topic.c_str(), payload.c_str());

  MqttMessage msg;
  msg.topic = topic;
  msg.payload.clear();
  msg.payload.reserve(payload.length());
  for (int i = 0; i < payload.length(); i++) {
    msg.payload.push_back(static_cast<uint8_t>(payload.charAt(i)));
  }
  msg.length = msg.payload.size();

  if (mqttQueueSub->push(msg, pdMS_TO_TICKS(10))) {
    log_d("URC: MQTT message queued, payload length %d", msg.length);
  } else {
    log_e("URC: MQTT queue full - dropping message");
  }

  consumeOkResponse(at->getStream());
}

void AsyncEG915U::onMqttStat(const String & /*urc*/) {
  log_w("URC: +QMTSTAT received");
  URCState.mqttState.store(MqttConnectionState::DISCONNECTED);
}
