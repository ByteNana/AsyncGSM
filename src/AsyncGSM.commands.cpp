#include "AsyncGSM.h"

bool AsyncGSM::gprsDisconnect() {
  // Deactivate the GPRS context
  String response;
  bool ok = at.sendCommand("AT+QIDEACT=1", response, "OK", 4000);
  return ok;
}

bool AsyncGSM::gprsConnect(const char *apn, const char *user, const char *pwd) {
  gprsDisconnect();

  String cmd = String("AT+QICSGP=1,1,\"") + apn +
               "\","
               "" +
               (user ? user : "") +
               "\","
               "" +
               (pwd ? pwd : "") + "\"";
  String response;
  if (!at.sendCommand(cmd, response, "OK", 1000)) {
    return false;
  }

  if (!at.sendCommand("AT+QIACT=1", response, "OK", 150000)) {
    return false;
  }

  if (!at.sendCommand("AT+CGATT=1", response, "OK", 60000)) {
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
