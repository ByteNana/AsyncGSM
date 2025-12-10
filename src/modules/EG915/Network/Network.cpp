#include "Network.h"

#include "esp_log.h"

EG915Network::EG915Network(AsyncATHandler &handler) { init(handler); }

void EG915Network::init(AsyncATHandler &handler) { at = &handler; }

RegStatus EG915Network::getRegistrationStatus() { return creg.load(); }

String EG915Network::getOperator() {
  String cops;
  if (!at->sendSync("AT+COPS?", cops)) {
    log_e("Failed to get operator");
    return "";
  }
  int firstQuote = cops.indexOf('"');
  if (firstQuote == -1) {
    log_e("No operator response found");
    return "";
  }
  int secondQuote = cops.indexOf('"', firstQuote + 1);
  if (secondQuote == -1) {
    log_e("No operator response found");
    return "";
  }
  String result = cops.substring(firstQuote + 1, secondQuote);
  return result;
}

bool EG915Network::isGPRSSAttached() {
  ATPromise *promise = at->sendCommand("AT+CGATT?");
  if (!promise->wait() || !promise->getResponse()->containsResponse("+CGATT: 1")) {
    log_e("Failed to get GPRS attach status");
    at->popCompletedPromise(promise->getId());
    return false;
  }
  at->popCompletedPromise(promise->getId());
  return true;
}

bool EG915Network::activatePDP() {
  ATPromise *promise = at->sendCommand("AT+CGACT=1,1");
  promise->timeout(20000);  // Activation can take longer
  promise->expect("+CGACT: 1,1");
  if (!promise->wait() || !promise->getResponse()->isSuccess()) {
    log_e("Failed to activate PDP context");
    at->popCompletedPromise(promise->getId());
    return false;
  }
  at->popCompletedPromise(promise->getId());
  return true;
}

bool EG915Network::setPDPContext(const char *apn) {
  ATPromise *promise = at->sendCommand("AT+CREG?");
  int attempts = 0;
  while (creg.load() != RegStatus::REG_OK_HOME && creg.load() != RegStatus::REG_OK_ROAMING) {
    log_v("Waiting for network registration...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    promise = at->sendCommand("AT+CREG?");
    if (!promise->wait() || !promise->getResponse()->isSuccess()) {
      log_e("Failed to get network registration status");
      at->popCompletedPromise(promise->getId());
      return false;
    }
    at->popCompletedPromise(promise->getId());
    attempts++;
    if (attempts >= 10) {
      log_e("Network registration timed out");
      return false;
    }
  }
  // Set the PDP context
  return at->sendSync(String("AT+CGDCONT=1,\"IP\",\"") + apn + "\"");
}

bool EG915Network::checkContext() {
  String r;
  if (!at->sendSync("AT+QIACT?", r)) return false;
  return r.indexOf("+QIACT: 1,1") != -1;
}

bool EG915Network::gprsConnect(const char *apn, const char *user, const char *pass) {
  gprsDisconnect();
  setPDPContext(apn);
  String cmd = String("AT+QICSGP=1,1,\"") + apn + "\",\"" + (user ? user : "") + "\",\"" +
               (pass ? pass : "") + "\",1";
  if (!at->sendSync(cmd)) {
    log_e("Failed to set APN");
    return false;
  }

  log_d("PDP context activated successfully");
  if (!isGPRSSAttached()) {
    log_e("GPRS is not attached after PDP context activation");
    return false;
  }
  log_d("GPRS is attached");
  if (!at->sendSync("AT+QIACT=1")) {
    log_e("Failed to activate PDP context");
    return false;
  }

  activatePDP();

  if (!checkContext()) { return false; }

  return true;
}

bool EG915Network::gprsDisconnect() {
  if (!at) return false;
  return at->sendSync("AT+QIDEACT=1");
}

bool EG915Network::attachGPRS() {
  log_d("Attaching GPRS...");
  if (!isGPRSSAttached()) {
    log_d("GPRS is not attached, trying to attach...");
    if (!at->sendSync("AT+CGATT=1")) {
      log_e("Failed to attach GPRS");
      return false;
    }
  }
  return true;
}

void EG915Network::setRegistrationStatus(RegStatus status) {
  log_v("URC: Registration status updated to %d", creg.load());
  creg.store(status);
}