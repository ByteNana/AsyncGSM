#include "EG915.h"
#include "esp_log.h"

AsyncEG915U::AsyncEG915U() {}

AsyncEG915U::~AsyncEG915U() {
  if (at) {
    // Reader task will be stopped by end(); URC handlers are ephemeral
    at->end();
  }
}

bool AsyncEG915U::init(Stream &stream, AsyncATHandler &atHandler,
                       std::deque<uint8_t> &rxBuf, SemaphoreHandle_t &mutex) {
  at = &atHandler;
  _stream = &stream;
  rxBuffer = &rxBuf;
  rxMutex = mutex;
  // Register dynamic URC handlers
  registerURCs();
  return true;
}

bool AsyncEG915U::setEchoOff() { return at->sendSync("ATE0"); }

bool AsyncEG915U::enableVerboseErrors() { return at->sendSync("AT+CMEE=2"); }

bool AsyncEG915U::checkModemModel() {
  String model;
  if (!at->sendSync("AT+CGMM", model)) {
    log_e("Failed to get modem model");
    return false;
  }
  if (model.indexOf("EG915U") == -1) {
    log_e("Modem model is not EG915U");
    return false;
  }
  String mfr;
  if (!at->sendSync("AT+CGMI", mfr)) {
    log_e("Failed to get modem manufacturer");
    return false;
  }
  if (mfr.indexOf("Quectel") == -1) {
    log_e("Modem manufacturer is not Quectel");
    return false;
  }
  return true;
}

bool AsyncEG915U::checkTimezone() { return at->sendSync("AT+CTZU=1"); }

bool AsyncEG915U::checkSIMReady() {
  String r;
  if (!at->sendSync("AT+CPIN?", r))
    return false;
  return r.indexOf("READY") != -1;
}

void AsyncEG915U::disableConnections() {
  for (int i = 0; i < 4; i++) {
    at->sendSync(String("AT+QICLOSE=") + String(i));
  }
}

bool AsyncEG915U::disalbeSleepMode() { return at->sendSync("AT+QSCLK=0"); }

bool AsyncEG915U::checkNetworkContext() {
  String r;
  if (!at->sendSync("AT+QIACT?", r))
    return false;
  return r.indexOf("+QIACT: 1,1") != -1;
}

bool AsyncEG915U::gprsConnect(const char *apn, const char *user,
                              const char *pass) {
  gprsDisconnect();
  setPDPContext(apn);
  String cmd = String("AT+QICSGP=1,1,\"") + apn + "\",\"" + (user ? user : "") +
               "\",\"" + (pass ? pass : "") + "\",1";
  if (!at->sendSync(cmd)) {
    log_e("Failed to set APN");
    return false;
  }

  log_i("PDP context activated successfully");
  if (!isGPRSSAttached()) {
    log_e("GPRS is not attached after PDP context activation");
    return false;
  }
  log_i("GPRS is attached");
  if (!at->sendSync("AT+QIACT=1")) {
    log_e("Failed to activate PDP context");
    return false;
  }

  activatePDP();

  if (!checkNetworkContext()) {
    return false;
  }

  return true;
}

bool AsyncEG915U::gprsDisconnect() { return at->sendSync("AT+QIDEACT=1"); }

bool AsyncEG915U::attachGPRS() {
  log_i("Attaching GPRS...");
  if (!isGPRSSAttached()) {
    log_i("GPRS is not attached, trying to attach...");
    if (!at->sendSync("AT+CGATT=1")) {
      log_e("Failed to attach GPRS");
      return false;
    }
  }
  return true;
}

String AsyncEG915U::getSimCCID() {
  String ccid;
  if (!at->sendSync("AT+CCID", ccid)) {
    log_e("Failed to get SIM CCID");
    return "";
  }
  int idx = ccid.indexOf("+QCCID:");
  if (idx == -1) {
    log_e("No +QCCID response found");
    return "";
  }
  String result = ccid.substring(idx + 7);
  result.trim();
  return result;
}

String AsyncEG915U::getIMEI() {
  String imei;
  if (!at->sendSync("AT+CGSN", imei)) {
    log_e("Failed to get IMEI");
    return "";
  }
  int idx = imei.indexOf("\r\n");
  if (idx == -1) {
    log_e("No IMEI response found");
    return "";
  }
  imei = imei.substring(idx);
  imei.trim();
  return imei;
}

String AsyncEG915U::getOperator() {
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

String AsyncEG915U::getIPAddress() {
  String qiact;
  if (!at->sendSync("AT+QIACT?", qiact)) {
    log_e("Failed to get IP address");
    return "";
  }
  int firstQuote = qiact.indexOf('"');
  if (firstQuote == -1) {
    log_e("No IP address response found");
    return "";
  }
  int secondQuote = qiact.indexOf('"', firstQuote + 1);
  if (secondQuote == -1) {
    log_e("No IP address response found");
    return "";
  }
  String result = qiact.substring(firstQuote + 1, secondQuote);
  return result;
}

bool AsyncEG915U::stop() { return at->sendSync("AT+QICLOSE=0"); }

bool AsyncEG915U::connect(const char *host, uint16_t port) {
  String portStr(port);
  URCState.isConnected.store(ConnectionStatus::DISCONNECTED);
  at->sendSync(String("AT+QIOPEN=1,0,\"TCP\",\"") + host + "\"," + portStr +
               ",0,0");

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
