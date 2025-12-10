#include "ModemInfo.h"

#include "esp_log.h"

static String querySingleLine(AsyncATHandler* at, const char* cmd, const char* what) {
  String resp;
  if (!at || !at->sendSync(cmd, resp)) {
    log_e("Failed to get %s", what);
    return "";
  }

  int idx = resp.indexOf("\r\n");
  if (idx == -1) {
    log_e("No %s response found", what);
    return "";
  }

  resp = resp.substring(idx);
  resp.trim();
  return resp;
}

String EG915ModemInfo::getModel() { return querySingleLine(at, "AT+CGMM", "modem model"); }

String EG915ModemInfo::getManufacturer() {
  return querySingleLine(at, "AT+CGMI", "modem manufacturer");
}

String EG915ModemInfo::getIMEI() { return querySingleLine(at, "AT+CGSN", "IMEI"); }

String EG915ModemInfo::getIMSI() { return querySingleLine(at, "AT+CIMI", "IMSI"); }

String EG915ModemInfo::getICCID() {
  String resp = querySingleLine(at, "AT+CCID", "ICCID");
  if (resp.startsWith("+QCCID: ")) { return resp.substring(8); }
  return resp;
}

String EG915ModemInfo::getIPAddress() {
  if (!at) {
    log_e("Failed to get IP address: AT handler is null");
    return "";
  }

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