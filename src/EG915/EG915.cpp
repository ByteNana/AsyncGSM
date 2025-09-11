#include "EG915.h"
#include "esp_log.h"

AsyncEG915U::AsyncEG915U() {}

AsyncEG915U::~AsyncEG915U() {
  if (at) {
    at->end();
  }
}

bool AsyncEG915U::init(Stream &stream, AsyncATHandler &atHandler,
                       std::deque<uint8_t> &rxBuf) {
  at = &atHandler;
  _stream = &stream;
  rxBuffer = &rxBuf;
  // Register our URC handler
  at->onURC([this](const String &urc) { this->handleURC(urc); });
  return true;
}

bool AsyncEG915U::setEchoOff() {
  ATPromise *promise = at->sendCommand("ATE0");
  if (!promise->wait() || !promise->getResponse()->isSuccess()) {
    log_e("Failed to disable echo");
    at->popCompletedPromise(promise->getId());
    return false;
  }
  at->popCompletedPromise(promise->getId());
  return true;
}

bool AsyncEG915U::enableVerboseErrors() {
  ATPromise *promise = at->sendCommand("AT+CMEE=2");
  if (!promise->wait() || !promise->getResponse()->isSuccess()) {
    log_e("Failed to enable verbose errors");
    at->popCompletedPromise(promise->getId());
    return false;
  }
  at->popCompletedPromise(promise->getId());
  return true;
}

bool AsyncEG915U::checkModemModel() {

  ATPromise *cgmm = at->sendCommand("AT+CGMM");
  if (!cgmm->wait() || !cgmm->getResponse()->isSuccess()) {
    log_e("Failed to get modem model");
    at->popCompletedPromise(cgmm->getId());
    return false;
  }

  ATResponse *cgmmResp = cgmm->getResponse();
  if (!cgmmResp->containsResponse("EG915U")) {
    log_e("Modem model is not EG915U");
    at->popCompletedPromise(cgmmResp->getId());
    return false;
  }

  at->popCompletedPromise(cgmmResp->getId());

  ATPromise *cgmi = at->sendCommand("AT+CGMI");
  if (!cgmi->wait() || !cgmi->getResponse()->isSuccess()) {
    log_e("Failed to get modem manufacturer");
    at->popCompletedPromise(cgmi->getId());
    return false;
  }
  ATResponse *cgmiResp = cgmi->getResponse();
  if (!cgmiResp->containsResponse("Quectel")) {
    log_e("Modem manufacturer is not Quectel");
    at->popCompletedPromise(cgmiResp->getId());
    return false;
  }
  at->popCompletedPromise(cgmiResp->getId());
  return true;
}

bool AsyncEG915U::checkTimezone() {
  ATPromise *promise = at->sendCommand("AT+CTZU=1");
  if (!promise->wait() || !promise->getResponse()->isSuccess()) {
    log_e("Failed to set time zone update");
    at->popCompletedPromise(promise->getId());
    return false;
  }
  at->popCompletedPromise(promise->getId());
  return true;
}

bool AsyncEG915U::checkSIMReady() {
  ATPromise *promise = at->sendCommand("AT+CPIN?");
  if (!promise->wait()) {
    log_e("Failed to get SIM status");
    at->popCompletedPromise(promise->getId());
    return false;
  }
  ATResponse *resp = promise->getResponse();
  if (!resp->containsResponse("READY")) {
    log_e("SIM is not ready");
    at->popCompletedPromise(resp->getId());
    return false;
  }
  at->popCompletedPromise(resp->getId());
  return true;
}

void AsyncEG915U::disableConnections() {
  for (int i = 0; i < 4; i++) {
    ATPromise *promise = at->sendCommand(String("AT+QICLOSE=") + String(i));
    promise->wait();
    at->popCompletedPromise(promise->getId());
  }
}

bool AsyncEG915U::disalbeSleepMode() {
  ATPromise *promise = at->sendCommand("AT+QSCLK=0");
  if (!promise->wait() || !promise->getResponse()->isSuccess()) {
    log_e("Failed to disable sleep mode");
    at->popCompletedPromise(promise->getId());
    return false;
  }
  at->popCompletedPromise(promise->getId());
  return true;
}

bool AsyncEG915U::checkNetworkContext() {
  // Check if the PDP context is active
  ATPromise *promise = at->sendCommand("AT+QIACT?");
  if (!promise->wait() || !promise->getResponse()->containsResponse("+QIACT: 1,1")) {
    log_e("PDP context is not active");
    at->popCompletedPromise(promise->getId());
    return false;
  }
  at->popCompletedPromise(promise->getId());
  return true;
}

bool AsyncEG915U::gprsConnect(const char *apn, const char *user,
                              const char *pass) {
  gprsDisconnect();
  setPDPContext(apn);
  String cmd = String("AT+QICSGP=1,1,\"") + apn + "\",\"" + (user ? user : "") +
               "\",\"" + (pass ? pass : "") + "\",1";
  ATPromise *promise = at->sendCommand(cmd);
  if (!promise->wait() || !promise->getResponse()->isSuccess()) {
    log_e("Failed to set APN");
    at->popCompletedPromise(promise->getId());
    return false;
  }
  at->popCompletedPromise(promise->getId());

  log_i("PDP context activated successfully");
  if (!isGPRSSAttached()) {
    log_e("GPRS is not attached after PDP context activation");
    return false;
  }
  log_i("GPRS is attached");
  promise = at->sendCommand("AT+QIACT=1");
  if (!promise->wait() || !promise->getResponse()->isSuccess()) {
    log_e("Failed to activate PDP context");
    at->popCompletedPromise(promise->getId());
    return false;
  }
  at->popCompletedPromise(promise->getId());

  activatePDP();

  if (!checkNetworkContext()) {
    return false;
  }

  return true;
}

bool AsyncEG915U::gprsDisconnect() {
  ATPromise *promise = at->sendCommand("AT+QIDEACT=1");
  if (!promise->wait() || !promise->getResponse()->isSuccess()) {
    log_e("Failed to deactivate PDP context");
    at->popCompletedPromise(promise->getId());
    return false;
  }
  at->popCompletedPromise(promise->getId());
  return true;
}

bool AsyncEG915U::attachGPRS() {
  log_i("Attaching GPRS...");
  if (!isGPRSSAttached()) {
    log_i("GPRS is not attached, trying to attach...");
    ATPromise *promise = at->sendCommand("AT+CGATT=1");
    if (!promise->wait() || !promise->getResponse()->isSuccess()) {
      log_e("Failed to attach GPRS");
      at->popCompletedPromise(promise->getId());
      return false;
    }
    at->popCompletedPromise(promise->getId());
  }
  return true;
}

String AsyncEG915U::getSimCCID() {
  ATPromise *promise = at->sendCommand("AT+CCID");
  if (!promise->expect("+QCCID:")->wait() ||
      !promise->getResponse()->isSuccess()) {
    log_e("Failed to get SIM CCID");
    at->popCompletedPromise(promise->getId());
    return "";
  }
  ATResponse *resp = promise->getResponse();
  String ccid = resp->getFullResponse();
  int idx = ccid.indexOf("+QCCID:");
  if (idx == -1) {
    log_e("No +QCCID response found");
    at->popCompletedPromise(resp->getId());
    return "";
  }
  String result = ccid.substring(idx + 7);
  result.trim();
  at->popCompletedPromise(resp->getId());
  return result;
}

String AsyncEG915U::getIMEI() {
  ATPromise *promise = at->sendCommand("AT+CGSN");
  if (!promise->wait() || !promise->getResponse()->isSuccess()) {
    log_e("Failed to get IMEI");
    at->popCompletedPromise(promise->getId());
    return "";
  }

  ATResponse *resp = promise->getResponse();
  String imei = resp->getFullResponse();
  int idx = imei.indexOf("\r\n");
  if (idx == -1) {
    log_e("No IMEI response found");
    at->popCompletedPromise(resp->getId());
    return "";
  }
  imei = imei.substring(idx);
  imei.trim();
  return imei;
}

String AsyncEG915U::getOperator() {
  ATPromise *promise = at->sendCommand("AT+COPS?");
  if (!promise->expect("+COPS:")->wait() ||
      !promise->getResponse()->isSuccess()) {
    log_e("Failed to get operator");
    at->popCompletedPromise(promise->getId());
    return "";
  }
  ATResponse *resp = promise->getResponse();
  String cops = resp->getFullResponse();
  int firstQuote = cops.indexOf('"');
  if (firstQuote == -1) {
    log_e("No operator response found");
    at->popCompletedPromise(resp->getId());
    return "";
  }
  int secondQuote = cops.indexOf('"', firstQuote + 1);
  if (secondQuote == -1) {
    log_e("No operator response found");
    at->popCompletedPromise(resp->getId());
    return "";
  }
  String result = cops.substring(firstQuote + 1, secondQuote);
  at->popCompletedPromise(resp->getId());
  return result;
}

String AsyncEG915U::getIPAddress() {
  ATPromise *promise = at->sendCommand("AT+QIACT?");
  if (!promise->expect("+QIACT:")->wait() ||
      !promise->getResponse()->isSuccess()) {
    log_e("Failed to get IP address");
    at->popCompletedPromise(promise->getId());
    return "";
  }
  ATResponse *resp = promise->getResponse();
  String qiact = resp->getFullResponse();
  int firstQuote = qiact.indexOf('"');
  if (firstQuote == -1) {
    log_e("No IP address response found");
    at->popCompletedPromise(resp->getId());
    return "";
  }
  int secondQuote = qiact.indexOf('"', firstQuote + 1);
  if (secondQuote == -1) {
    log_e("No IP address response found");
    at->popCompletedPromise(resp->getId());
    return "";
  }
  String result = qiact.substring(firstQuote + 1, secondQuote);
  at->popCompletedPromise(resp->getId());
  return result;
}

bool AsyncEG915U::connect(const char *host, uint16_t port) {
  String portStr(port);
  URCState.isConnected.store(ConnectionStatus::DISCONNECTED);
  ATPromise *promise = at->sendCommand(String("AT+QIOPEN=1,0,\"TCP\",\"") +
                                       host + "\"," + portStr + ",0,0");

  at->popCompletedPromise(promise->getId());

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
