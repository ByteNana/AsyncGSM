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
    at->getStream()->print("AT+QIRD=0");
    at->getStream()->print("\r\n");
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

    int dataLen = header.toInt();
    if (dataLen <= 0) {
      log_e(">>>>>>>>>>>QIRD: Invalid length");
      return;
    }
    log_d("QIRD: Data length = %d", dataLen);

    while (at->getStream()->available()) {
      char c = at->getStream()->read();
      if (dataLen >= 0) {
        rxBuffer->push_back(c);
      } else {
        printf("QIRD: Extra byte received: %c\n", c);
      }
      dataLen--;
    }

    log_i("QIRD: Received %d bytes", (int)rxBuffer->size());
    for (uint8_t b : *rxBuffer) {
      printf("%c", b);
    }
    printf("\n");
    if (rxBuffer->size() < dataLen) {
      at->getStream()->print("AT+QIRD=0");
      at->getStream()->print("\r\n");
      at->getStream()->flush();
    }
  }
}
