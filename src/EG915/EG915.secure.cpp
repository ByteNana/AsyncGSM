#include "EG915.h"
#include "esp_log.h"

bool AsyncEG915U::connectSecure(const char *host, uint16_t port) {
  // Enable SSL TSL 1.2
  ATPromise *sslPromise = at->sendCommand("AT+QSSLCFG=\"sslversion\",1,3");
  if (!sslPromise->wait()) {
    log_e("Failed to set SSL version");
    at->popCompletedPromise(sslPromise->getId());
    return false;
  }
  at->popCompletedPromise(sslPromise->getId());

  // Create SSL context - TLS_RSA_WITH_AES_256_CBC_SHA
  sslPromise = at->sendCommand("AT+QSSLCFG=\"ciphersuite\",1,0X0035");
  if (!sslPromise->wait()) {
    log_e("Failed to set SSL cipher suite");
    at->popCompletedPromise(sslPromise->getId());
    return false;
  }

  // Set security level to 1 (no client verification)
  at->popCompletedPromise(sslPromise->getId());
  sslPromise = at->sendCommand("AT+QSSLCFG=\"seclevel\",1,1");
  if (!sslPromise->wait()) {
    log_e("Failed to set SSL security level");
    at->popCompletedPromise(sslPromise->getId());
    return false;
  }
  at->popCompletedPromise(sslPromise->getId());

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
  sslPromise = at->sendCommand(String("AT+QSSLOPEN=1,1,0,\"") + host + "\"," +
                               String(port) + ",0");
  if (!sslPromise->wait()) {
    log_e("Failed to open SSL connection");
    at->popCompletedPromise(sslPromise->getId());
    return false;
  }
  at->popCompletedPromise(sslPromise->getId());

  for (int i = 0; i < 20; i++) {
    ConnectionStatus status = URCState.isConnected.load();
    if (status == ConnectionStatus::CONNECTED ||
        status == ConnectionStatus::FAILED) {
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  if (URCState.isConnected.load() == ConnectionStatus::CONNECTED) {
    log_i("Connection URC received successfully");
  } else {
    log_e("No +QIOPEN URC received or it indicated failure");
  }
  return URCState.isConnected.load() == ConnectionStatus::CONNECTED;
}
