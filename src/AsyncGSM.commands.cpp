#include "AsyncGSM.h"

bool AsyncGSM::gprsDisconnect() {
  // Deactivate the GPRS context
  bool ok = at.sendCommand("AT+QIDEACT=1", "OK");
  return ok;
}

bool AsyncGSM::gprsConnect(const char *apn, const char *user, const char *pwd) {
  gprsDisconnect();

  String response;
  if (!at.sendCommand(response, "OK", 1000, "AT+QICSGP=1,1,\"", apn, "\",\"",
                      (user && *user) ? user : "", "\", \"",
                      (pwd && *pwd) ? pwd : "", "\"")) {
    return false;
  }

  if (!at.sendCommand("AT+QIACT=1", "OK")) {
    return false;
  }

  if (!at.sendCommand("AT+CGATT=1", "OK")) {
    return false;
  }

  return true;
}

String AsyncGSM::getSimCCID() {
  String response;
  if (!at.sendCommand("AT+QCCID", response, "+QCCID:", 2000)) {
    return "";
  }

  int idx = response.indexOf("+QCCID:");
  if (idx == -1)
    return "";
  String rest = response.substring(idx);
  int colon = rest.indexOf(':');
  if (colon == -1)
    return "";
  String ccid = rest.substring(colon + 1);
  int end = ccid.indexOf("\r\n");
  if (end != -1)
    ccid = ccid.substring(0, end);
  ccid.trim();
  return ccid;
}

String AsyncGSM::getIMEI() {
  String response;
  if (!at.sendCommand("AT+CGSN", response, "OK", 2000)) {
    return "";
  }
  int start = response.indexOf("\r\n");
  if (start == -1)
    return "";
  String rest = response.substring(start + 2);
  int end = rest.indexOf("\r\n");
  if (end != -1)
    rest = rest.substring(0, end);
  rest.trim();
  return rest;
}

String AsyncGSM::getOperator() {
  String response;
  if (!at.sendCommand("AT+COPS?", response, "+COPS:", 2000)) {
    return "";
  }
  int idx = response.indexOf("+COPS:");
  if (idx == -1)
    return "";
  String rest = response.substring(idx);
  int firstQuote = rest.indexOf('"');
  if (firstQuote == -1)
    return "";
  String sub = rest.substring(firstQuote + 1);
  int secondQuote = sub.indexOf('"');
  if (secondQuote == -1)
    return "";
  String op = sub.substring(0, secondQuote);
  op.trim();
  return op;
}

String AsyncGSM::getIPAddress() {
  String response;
  if (!at.sendCommand("AT+CGPADDR=1", response, "+CGPADDR:", 2000)) {
    return "";
  }
  int idx = response.indexOf("+CGPADDR:");
  if (idx == -1)
    return "";
  String rest = response.substring(idx);
  int comma = rest.indexOf(',');
  if (comma == -1)
    return "";
  String ip = rest.substring(comma + 1);
  int end = ip.indexOf("\r\n");
  if (end != -1)
    ip = ip.substring(0, end);
  ip.trim();
  return ip;
}

bool AsyncGSM::modemConnect(const char *host, uint16_t port) {
  String response;
  // AT+QIOPEN=1,0,"TCP","220.180.239.212",8009,0,0
  // <PDPcontextID>(1-16), <connectID>(0-11),
  // "TCP/UDP/TCP LISTENER/UDPSERVICE", "<IP_address>/<domain_name>",
  // <remote_port>,<local_port>,<access_mode>(0-2; 0=buffer)
  String portStr(port);
  if (!at.sendCommand(response, "+QIOPEN:", 150000, "AT+QIOPEN=1,0,\"TCP\",\"",
                      host, "\",", portStr.c_str(), ",0,0")) {
    return false;
  }
  int idx = response.indexOf("+QIOPEN:");
  if (idx == -1) {
    return false;
  }
  String rest = response.substring(idx);
  int comma = rest.indexOf(',');
  if (comma == -1) {
    return false;
  }
  String errStr = rest.substring(comma + 1);
  errStr.trim();
  return errStr.startsWith("0");
}
