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
    at->getStream()->print("AT+QSSLRECV=0, 1500\r\n");
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
}
