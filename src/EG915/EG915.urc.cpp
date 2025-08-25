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
      URCState.creg = (RegStatus)statusStr.toInt();
      log_i("URC: Registration status updated to %d", URCState.creg);
    }
  }
  if (trimmed.startsWith("+QIOPEN:")) {
    log_d("URC: +QIOPEN received");
    int firstComma = trimmed.indexOf(',');
    if (firstComma != -1) {
      String resultStr = trimmed.substring(firstComma + 1);
      int endPos = resultStr.indexOf(',');
      if (endPos != -1)
        resultStr = resultStr.substring(0, endPos);
      int result = resultStr.toInt();
      if (result == 0) {
        URCState.isConnected = 1;
        log_i("URC: Connection opened successfully");
      } else {
        URCState.isConnected = 0;
        log_e("URC: Connection failed with error %d", result);
      }
    }
  }

  if (trimmed.startsWith("+QICLOSE:")) {
    URCState.isConnected = 0;
    log_i("URC: Connection closed");
  }

  if (trimmed.startsWith("+QIURC: \"recv\"")) {
    rxBuffer->clear();
    // +QIURC: "recv"
    // Indicates that data has been received and is ready to be read with +QIRD
    log_i("URC: Data received, ready to read with +QIRD");
    at->getStream()->print("AT+QIRD=0");
    at->getStream()->print("\r\n");
    at->getStream()->flush();
  }

  if (trimmed.startsWith("+QIRD:")) {
#ifdef ON_UNIT_TESTS
    if (rxBuffer->length())
      return;
#endif
    rxBuffer->clear();
    // +QIRD: <len>
    // Example: +QIRD: 728

    int headerStart = trimmed.indexOf(':') + 1;
    if (headerStart == 0) {
      log_e(">>>>>>>>>>>>URC: Failed to parse data length from +QIURC");
      return;
    }
    String header = trimmed.substring(headerStart);
    header.trim();

    int dataLen = header.toInt();
    if (dataLen <= 0) {
      log_e(">>>>>>>>>>>QIRD: Invalid length");
      return;
    }
    log_d("QIRD: Data length = %d", dataLen);

    // Find the start of the data itself.
    int dataStart = urc.indexOf('\n');
    if (dataStart == -1) {
      log_e(">>>>>>>>>URC: Failed to find data in +QIURC");
      return;
    }

    String data = urc.substring(dataStart + 1);
    data.replace("\"", "");
    (*rxBuffer) += data;

    int toRead = dataLen - data.length() - 1;
    for (int i = 0; i < toRead; i++) {
      char c = (char)at->getStream()->read();
      (*rxBuffer) += c;
    }

    log_i("QIRD: Received %d bytes", rxBuffer->length());
    log_d("QIRD: Data content: %s", rxBuffer->c_str());

    String remaining;
    for (int i = 0; i < 3; i++) {
      char c = (char)at->getStream()->read();
      remaining += c;
    }
    log_d("QIRD: Remaining data after read: %s", remaining.c_str());
  }
}
