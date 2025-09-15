#include "EG915.h"
#include "esp_log.h"
#include <MqttQueue/MqttQueue.h>

static bool consumeOkResponse(Stream *stream) {
  String tail = "";
  unsigned long startTime = millis();
  const unsigned long timeout = 5000; // 5 second timeout

  while (millis() - startTime < timeout) {
    if (stream->available()) {
      int c = stream->read();
      if (c >= 0) {
        uint8_t byte = static_cast<uint8_t>(c);

        // Check for terminator
        tail += (char)byte;
        if (tail.endsWith("\r\nOK\r\n")) {
          log_d("Found OK response");
          return true;
        }

        // Prevent tail string from growing without bound
        if (tail.length() > 8) {
          tail.remove(0, tail.length() - 8);
        }
      }
    } else {
      // No data available, small delay to avoid busy waiting
      delay(1);
    }
  }

  log_w("Timeout waiting for OK response");
  return false;
}

void AsyncEG915U::handleURC(const String &urc) {
  String trimmed = urc;
  trimmed.trim();

  log_d("Received URC: %s", trimmed.c_str());
  // Parse registration URCs and update the status
  if (trimmed.startsWith("+CREG:") || trimmed.startsWith("+CGREG:") ||
      trimmed.startsWith("+CEREG:")) {
    int commaIndex = trimmed.indexOf(',');
    if (commaIndex != -1) {
      String statusStr = trimmed.substring(commaIndex + 1);
      int endPos = statusStr.indexOf(',');
      if (endPos != -1)
        statusStr = statusStr.substring(0, endPos);
      URCState.creg.store((RegStatus)statusStr.toInt());
      log_i("URC: Registration status updated to %d", URCState.creg.load());
    }
  }
  if (trimmed.startsWith("+QIOPEN:") || trimmed.startsWith("+QSSLOPEN:")) {
    log_d("URC: +QIOPEN received");
    rxBuffer->clear();
    int firstComma = trimmed.indexOf(',');
    if (firstComma != -1) {
      String resultStr = trimmed.substring(firstComma + 1);
      int endPos = resultStr.indexOf(',');
      if (endPos != -1)
        resultStr = resultStr.substring(0, endPos);
      int result = resultStr.toInt();
      if (result == 0) {
        URCState.isConnected.store(ConnectionStatus::CONNECTED);
        log_i("URC: Connection opened successfully");
      } else {
        URCState.isConnected.store(ConnectionStatus::FAILED);
        log_e("URC: Connection failed with error %d", result);
      }
    }
  }

  if (trimmed.startsWith("+QICLOSE:") ||
      trimmed.startsWith("+QSSLURC: \"closed\"")) {
    URCState.isConnected.store(ConnectionStatus::DISCONNECTED);
    log_i("URC: Connection closed");
  }

  if (trimmed.startsWith("+QIURC: \"recv\"")) {
    // +QIURC: "recv"
    // Indicates that data has been received and is ready to be read with +QIRD
    log_i("URC: Data received, ready to read with +QIRD");
    // Ask modem to read pending data for socket 0
    at->getStream()->print("AT+QIRD=0\r\n");
    at->getStream()->flush();
  }

  if (trimmed.startsWith("+QSSLURC: \"recv\"")) {
    // +QSSLURC: "recv"
    // Indicates that data has been received and is ready to be read with
    // +QSSLRECV
    log_i("URC: SSL Data received, ready to read with +QIRD");
    // Ask modem to read pending data for socket 0
    at->getStream()->print("AT+QSSLRECV=0,1500\r\n");
    at->getStream()->flush();
  }

  if (trimmed.startsWith("+QIRD:") || trimmed.startsWith("+QSSLRECV:")) {
    // +QIRD: <len>
    // Example: +QIRD: 728
    int headerStart = urc.indexOf(':');
    if (headerStart == -1) {
      log_e(">>>>>>>>>>>>URC: Failed to parse data length from +QIURC");
      return;
    }
    String header = urc.substring(headerStart + 1);
    int remaining = header.toInt();
    if (remaining <= 0) {
      log_e(">>>>>>>>>>>QIRD: Invalid length");
      return;
    }
    log_d("QIRD: Data length = %d", remaining);
    // Pull exactly <remaining> bytes from the stream into the TCP rx buffer
    unsigned long lastByteTs = millis();
    String tail; // Move tail tracking to main loop
    while (remaining > 0) {
      if (at->getStream()->available()) {
        int c = at->getStream()->read();
        if (c >= 0) {
          uint8_t byte = static_cast<uint8_t>(c);
          rxBuffer->push_back(byte);
          remaining--;
          lastByteTs = millis();

          // Check for terminator
          tail += (char)byte;
          if (tail.endsWith("\r\nOK\r\n")) {
            // Remove the terminator from rxBuffer
            for (int i = 0; i < 6; i++) { // "\r\nOK\r\n" is 6 bytes
              if (!rxBuffer->empty())
                rxBuffer->pop_back();
            }
            break;
          }
          // Prevent tail string from growing without bound
          if (tail.length() > 8) {
            tail.remove(0, tail.length() - 8);
          }
        }
      } else {
        // Yield a tick and enforce a simple timeout to avoid stalling forever
        vTaskDelay(pdMS_TO_TICKS(1));
        if (millis() - lastByteTs > 5000) {
          log_w("QIRD: Timed out while reading payload, %d bytes remain",
                remaining);
          break;
        }
      }
    }
    // No need for separate tail reading section anymore
    log_d("QIRD: Appended bytes, rxBuffer size now %d", (int)rxBuffer->size());
  }

  if (urc.startsWith("+QMTRECV:")) {
    log_w("URC: +QMTRECV received");
    int commaCount = std::count(urc.begin(), urc.end(), ',');
    int headerStart = urc.indexOf(':');
    int firstComma = urc.indexOf(',');
    String afterFirstComma = urc.substring(firstComma + 1);
    afterFirstComma.trim();
    if (headerStart == -1 || firstComma == -1 ||
        afterFirstComma.length() == 0) {
      log_e("URC: Failed to parse +QMTRECV header");
      return;
    }

    String clientId = urc.substring(headerStart + 1, firstComma);
    int numEnd = 0;
    while (numEnd < afterFirstComma.length() &&
           isDigit(afterFirstComma.charAt(numEnd))) {
      numEnd++;
    }
    String msgId = afterFirstComma.substring(0, numEnd);
    log_i("URC: MQTT message for client %d on topic ID %d", clientId.toInt(),
          msgId.toInt());
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

    // Find topic: starts after second comma, enclosed in quotes
    int secondComma = urc.indexOf(',', firstComma + 1);
    if (secondComma == -1) {
      log_e("URC: Failed to find topic");
      return;
    }

    // Find first quote after second comma (topic start)
    int topicStart = urc.indexOf('"', secondComma + 1);
    if (topicStart == -1) {
      log_e("URC: Failed to find topic start quote");
      return;
    }

    // Find next quote (topic end)
    int topicEnd = urc.indexOf('"', topicStart + 1);
    if (topicEnd == -1) {
      log_e("URC: Failed to find topic end quote");
      return;
    }

    String topic = urc.substring(topicStart + 1, topicEnd);

    // Find payload: first quote after topic end to last quote in string
    int payloadStart = urc.indexOf('"', topicEnd + 1);
    if (payloadStart == -1) {
      log_e("URC: Failed to find payload start quote");
      return;
    }

    // Find last quote in the entire string (payload end)
    int payloadEnd = urc.lastIndexOf('"');
    if (payloadEnd <= payloadStart) {
      log_e("URC: Failed to find payload end quote");
      return;
    }

    String payload = urc.substring(payloadStart + 1, payloadEnd);

    log_i("URC: Topic: '%s', Payload: '%s'", topic.c_str(), payload.c_str());

    MqttMessage msg;
    msg.topic = topic;
    // Convert String payload to vector<uint8_t>
    msg.payload.clear();
    msg.payload.reserve(payload.length());
    for (int i = 0; i < payload.length(); i++) {
      msg.payload.push_back(static_cast<uint8_t>(payload.charAt(i)));
    }
    msg.length = msg.payload.size();

    if (mqttQueueSub->push(msg, pdMS_TO_TICKS(10))) {
      log_i("URC: MQTT message queued, payload length %d", msg.length);
    } else {
      log_e("URC: MQTT queue full - dropping message");
    }

    consumeOkResponse(at->getStream());
  }

  if (trimmed.startsWith("+QMTSTAT:")) {
    log_w("URC: +QMTSTAT received");
    URCState.mqttState.store(MqttConnectionState::DISCONNECTED);
  }
}
