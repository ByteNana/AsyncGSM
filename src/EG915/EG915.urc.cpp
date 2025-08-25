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
        URCState.isConnected = -1;
        log_e("URC: Connection failed with error %d", result);
      }
    }
  }

  if (trimmed.startsWith("+QICLOSE:")) {
    URCState.isConnected = 0;
    log_i("URC: Connection closed");
  }

  if (trimmed.startsWith("+QIURC: \"recv\"")) {
    // Find the start of the data length.
    int lenStart = trimmed.indexOf(',', trimmed.indexOf(',') + 1);
    if (lenStart == -1) {
      log_e("URC: Failed to parse data length from +QIURC");
      return;
    }

    // Parse the length of the incoming data.
    String lenStr = trimmed.substring(lenStart + 1);
    int lenEnd = lenStr.indexOf(',');
    if (lenEnd != -1) {
      lenStr = lenStr.substring(0, lenEnd);
    }
    int dataLen = lenStr.toInt();

    log_d("URC: Data size: %d", dataLen);

    // Find the start of the data itself.
    int dataStart = trimmed.indexOf(',', lenStart + 1);
    dataStart = trimmed.indexOf(',', dataStart + 1);
    dataStart = trimmed.indexOf('"', dataStart + 1);
    if (dataStart == -1) {
      log_e("URC: Failed to find data in +QIURC");
      return;
    }

    // Extract the data string.
    rxBuffer->clear();
    String data = urc.substring(dataStart + 1);
    data.replace("\"", "");
    (*rxBuffer) += data;

    int toRead = dataLen - data.length() - 1;
    for (int i = 0; i < toRead; i++) {
      char c = (char)at->getStream()->read();
      (*rxBuffer) += c;
    }

    log_i("URC: Received %u bytes of data", rxBuffer->length());
    log_d("URC: Data content: %s", rxBuffer->c_str());
  }
}
