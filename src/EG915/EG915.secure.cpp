#include "EG915.h"
#include "esp_log.h"
#include <MD5Builder.h>

bool AsyncEG915U::connectSecure(const char *host, uint16_t port) {
  // Enable TLS 1.2
  if (!at->sendSync("AT+QSSLCFG=\"sslversion\",1,3")) {
    log_e("Failed to set SSL version");
    return false;
  }

  // Allow a strong default cipher suite (RSA with AES-256-CBC-SHA)
  if (!at->sendSync("AT+QSSLCFG=\"ciphersuite\",1,0X0035")) {
    log_e("Failed to set SSL cipher suite");
    return false;
  }

  // Enable SNI so the server can select proper certificate when using hostnames
  at->sendSync("AT+QSSLCFG=\"sni\",1,1");

  // Set security level: 1 when CA is configured, else 0 (insecure)
  if (!at->sendSync(String("AT+QSSLCFG=\"seclevel\",1,") +
                    (certConfigured ? "1" : "0"))) {
    log_e("Failed to set SSL security level");
    return false;
  }

  // Open SSL connection (PDP ctx=1, SSL ctx=1, clientid=0)
  URCState.isConnected.store(ConnectionStatus::DISCONNECTED);
  at->sendSync(String("AT+QSSLOPEN=1,1,0,\"") + host + "\"," + String(port) +
               ",0");

  for (int i = 0; i < 20; i++) {
    ConnectionStatus status = URCState.isConnected.load();
    if (status == ConnectionStatus::CONNECTED ||
        status == ConnectionStatus::FAILED) {
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  if (URCState.isConnected.load() == ConnectionStatus::CONNECTED) {
    log_d("Connection URC received successfully");
  } else {
    log_e("No +QIOPEN URC received or it indicated failure");
  }
  return URCState.isConnected.load() == ConnectionStatus::CONNECTED;
}

bool AsyncEG915U::stopSecure() {
  if (!at->sendSync("AT+QSSLCLOSE=0")) {
    log_e("Failed to close SSL connection or already closed");
    return false;
  }
  URCState.isConnected.store(ConnectionStatus::DISCONNECTED);
  return true;
}

bool AsyncEG915U::uploadUFSFile(const char *path, const uint8_t *data,
                                size_t size, uint32_t timeoutMs) {
  if (!path || !*path || !data || size == 0) {
    log_e("Invalid params for uploadUFSFile");
    return false;
  }

  // Quectel expects timeout in seconds for QFUPL
  uint32_t timeoutSec = (timeoutMs + 999) / 1000;

  // Start the upload
  String cmd = String("AT+QFUPL=\"") + path + "\"," + String(size) + "," +
               String(timeoutSec);
  ATPromise *promise = at->sendCommand(cmd);
  if (!promise) {
    log_e("Failed to create promise for QFUPL");
    return false;
  }

  // Wait for the upload prompt (module sends CONNECT)
  if (!promise->expect("CONNECT")->wait()) {
    log_e("No CONNECT prompt for QFUPL");
    at->popCompletedPromise(promise->getId());
    return false;
  }

  // Send file bytes
  at->getStream()->write(data, size);
  at->getStream()->flush();

  // Wait for command completion (OK)
  bool ok = promise->wait();
  auto p = at->popCompletedPromise(promise->getId());
  if (!ok || !p || !p->getResponse()->isSuccess()) {
    log_e("QFUPL failed or not acknowledged");
    return false;
  }

  return true;
}

bool AsyncEG915U::setCACertificate(const char *ufsPath) {
  if (!ufsPath || !*ufsPath) {
    log_e("Invalid CA cert path");
    return false;
  }
  String cmd = String("AT+QSSLCFG=\"cacert\",1,\"") + ufsPath + "\"";
  if (!at->sendSync(cmd)) {
    log_e("Failed to set CA certificate path");
    return false;
  }
  if (!at->sendSync("AT+QSSLCFG=\"seclevel\",1,1")) {
    log_e("Failed to set SSL seclevel to 1");
    return false;
  }
  certConfigured = true;
  return true;
}

bool AsyncEG915U::findUFSFile(const char *pattern, String *outName,
                              size_t *outSize) {
  if (!pattern || !*pattern)
    return false;
  String resp;
  String cmd = String("AT+QFLST=\"") + pattern + "\"";
  if (!at->sendSync(cmd, resp)) {
    return false;
  }
  int pos = resp.indexOf("+QFLST: ");
  if (pos == -1) {
    return false; // no matches
  }
  // Parse first match: +QFLST: "<name>",<size>
  int q1 = resp.indexOf('"', pos);
  if (q1 == -1)
    return false;
  int q2 = resp.indexOf('"', q1 + 1);
  if (q2 == -1)
    return false;
  String name = resp.substring(q1 + 1, q2);
  if (outName)
    *outName = name;
  if (outSize) {
    int comma = resp.indexOf(',', q2);
    if (comma != -1) {
      // read until end of line or next CR
      int lineEnd = resp.indexOf('\n', comma);
      String sizeStr = (lineEnd == -1) ? resp.substring(comma + 1)
                                       : resp.substring(comma + 1, lineEnd);
      sizeStr.trim();
      *outSize = static_cast<size_t>(sizeStr.toInt());
    }
  }
  return true;
}
