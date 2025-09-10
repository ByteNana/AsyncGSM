#include "EG915.h"
#include "esp_log.h"

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
  if (trimmed.startsWith("+QIOPEN:")) {
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

  if (trimmed.startsWith("+QICLOSE:")) {
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

  if (trimmed.startsWith("+QIRD:")) {
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
    while (remaining > 0) {
      if (at->getStream()->available()) {
        int c = at->getStream()->read();
        if (c >= 0) {
          rxBuffer->push_back(static_cast<uint8_t>(c));
          remaining--;
          lastByteTs = millis();
        }
      } else {
        // Yield a tick and enforce a simple timeout to avoid stalling forever
        vTaskDelay(pdMS_TO_TICKS(1));
        if (millis() - lastByteTs > 5000) {
          log_w("QIRD: Timed out while reading payload, %d bytes remain", remaining);
          break;
        }
      }
    }

    // Discard trailing "\r\nOK\r\n" coming from AT+QIRD to avoid polluting the TCP stream
    String tail;
    unsigned long tailStart = millis();
    while (millis() - tailStart < 50) {
      if (!at->getStream()->available()) {
        vTaskDelay(pdMS_TO_TICKS(1));
        continue;
      }
      char c = at->getStream()->read();
      tail += c;
      if (tail.endsWith("\r\nOK\r\n")) {
        break;
      }
      // Prevent tail string from growing without bound
      if (tail.length() > 8) {
        tail.remove(0, tail.length() - 8);
      }
    }

    log_d("QIRD: Appended bytes, rxBuffer size now %d", (int)rxBuffer->size());
  }
}
