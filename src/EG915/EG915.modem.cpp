#include "EG915.h"
#include "esp_log.h"

bool AsyncEG915U::setPDPContext(const char *apn) {
  ATPromise *promise = at->sendCommand("AT+CREG?");
  while (URCState.creg.load() != RegStatus::REG_OK_HOME &&
         URCState.creg.load() != RegStatus::REG_OK_ROAMING) {
    log_i("Waiting for network registration...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    promise = at->sendCommand("AT+CREG?");
    if (!promise->wait() || !promise->getResponse()->isSuccess()) {
      log_e("Failed to get network registration status");
      at->popCompletedPromise(promise->getId());
      return false;
    }
    at->popCompletedPromise(promise->getId());
  }
  // Set the PDP context
  return at->sendSync(String("AT+CGDCONT=1,\"IP\",\"") + apn + "\"");
}

bool AsyncEG915U::isGPRSSAttached() {
  ATPromise *promise = at->sendCommand("AT+CGATT?");
  if (!promise->wait() || !promise->getResponse()->containsResponse("+CGATT: 1")) {
    log_e("Failed to get GPRS attach status");
    at->popCompletedPromise(promise->getId());
    return false;
  }
  at->popCompletedPromise(promise->getId());
  return true;
}

bool AsyncEG915U::activatePDP() {
  ATPromise *promise = at->sendCommand("AT+CGACT=1,1");
  promise->timeout(20000); // Activation can take longer
  promise->expect("+CGACT: 1,1");
  if (!promise->wait() || !promise->getResponse()->isSuccess()) {
    log_e("Failed to activate PDP context");
    at->popCompletedPromise(promise->getId());
    return false;
  }
  at->popCompletedPromise(promise->getId());
  return true;
}

