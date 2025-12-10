#include "Socket.h"

bool EG915Socket::uploadUFSFile(
    const char *path, const uint8_t *data, size_t size, uint32_t timeoutMs) {
  if (!at) {
    log_e("AT handler not initialized");
    return false;
  }
  if (!path || !*path || !data || size == 0) {
    log_e("Invalid params for uploadUFSFile");
    return false;
  }

  // Quectel expects timeout in seconds for QFUPL
  uint32_t timeoutSec = (timeoutMs + 999) / 1000;

  // Start the upload
  String cmd = String("AT+QFUPL=\"") + path + "\"," + String(size) + "," + String(timeoutSec);
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

bool EG915Socket::setCACertificate(const char *ufsPath, const char *ssl_cidx) {
  if (!at) {
    log_e("AT handler not initialized");
    return false;
  }

  if (!ufsPath || !*ufsPath) {
    log_e("Invalid CA cert path");
    return false;
  }
  String cmd = String("AT+QSSLCFG=\"cacert\",") + ssl_cidx + ",\"" + ufsPath + "\"";
  if (!at->sendSync(cmd)) {
    log_e("Failed to set CA certificate path");
    return false;
  }
  cmd = String("AT+QSSLCFG=\"seclevel\",") + ssl_cidx + ",1";
  if (!at->sendSync(cmd)) {
    log_e("Failed to set SSL seclevel to 1");
    return false;
  }
  certConfigured = true;
  return true;
}

bool EG915Socket::findUFSFile(const char *pattern, String *outName, size_t *outSize) {
  if (!at) {
    log_e("AT handler not initialized");
    return false;
  }

  if (!pattern || !*pattern) return false;
  String resp;
  String cmd = String("AT+QFLST=\"") + pattern + "\"";
  if (!at->sendSync(cmd, resp)) { return false; }
  int pos = resp.indexOf("+QFLST: ");
  if (pos == -1) {
    return false;  // no matches
  }
  // Parse first match: +QFLST: "<name>",<size>
  int q1 = resp.indexOf('"', pos);
  if (q1 == -1) return false;
  int q2 = resp.indexOf('"', q1 + 1);
  if (q2 == -1) return false;
  String name = resp.substring(q1 + 1, q2);
  if (outName) *outName = name;
  if (outSize) {
    int comma = resp.indexOf(',', q2);
    if (comma != -1) {
      // read until end of line or next CR
      int lineEnd = resp.indexOf('\n', comma);
      String sizeStr =
          (lineEnd == -1) ? resp.substring(comma + 1) : resp.substring(comma + 1, lineEnd);
      sizeStr.trim();
      *outSize = static_cast<size_t>(sizeStr.toInt());
    }
  }
  return true;
}
