#include "EG915.h"
#include "esp_log.h"

bool AsyncEG915U::connectSecure(const char *host, uint16_t port) {
  // Enable SSL TLS 1.2
  if (!at->sendSync("AT+QSSLCFG=\"sslversion\",1,3")) {
    log_e("Failed to set SSL version");
    return false;
  }

  // Create SSL context - TLS_RSA_WITH_AES_256_CBC_SHA
  if (!at->sendSync("AT+QSSLCFG=\"ciphersuite\",1,0X0035")) {
    log_e("Failed to set SSL cipher suite");
    return false;
  }

  // Set security level to 0 (Insecure for now)
  // TODO: Change to 1 after implementing certificate handling
  if (!at->sendSync("AT+QSSLCFG=\"seclevel\",1,0")) {
    log_e("Failed to set SSL security level");
    return false;
  }

  log_w("TODO: Implement certificate handling");
  // Set the certificate if available
  // sslPromise = at->sendCommand("AT+QSSLCFG=\"cacert\",0,\"my_ca.pem\"");
  // if (!sslPromise->wait()) {
  //   log_e("Failed to set CA certificate");
  //   at->popCompletedPromise(sslPromise->getId());
  //   return false;
  // }

  // Open ssl connection
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
