#include "AsyncGSM.h"
#include "esp_log.h"

bool stringContains(const String &str, const String &sub) {
  return str.indexOf(sub) != -1;
}

int8_t AsyncGSM::getRegistrationStatusXREG(const char *regCommand) {
  String response;
  at.sendCommand(response, "OK", 5000, "AT+", regCommand, "?");

  if (response.indexOf("+CREG:") == -1 && response.indexOf("+CGREG:") == -1 &&
      response.indexOf("+CEREG:") == -1) {
    return REG_NO_RESULT; // Error in command
  }

  int commaIndex = response.indexOf(',');
  if (commaIndex == -1) {
    return REG_NO_RESULT; // No registration result
  }

  String statusStr = response.substring(commaIndex + 1);
  // Find end of status (space, comma, or end of string)
  int endPos = statusStr.indexOf(',');
  if (endPos == -1) endPos = statusStr.indexOf(' ');
  if (endPos == -1) endPos = statusStr.indexOf('\r');
  if (endPos == -1) endPos = statusStr.indexOf('\n');
  if (endPos != -1) {
    statusStr = statusStr.substring(0, endPos);
  }

  statusStr.trim();
  log_d("Registration response: %s, status: %s", response.c_str(), statusStr.c_str());
  return statusStr.toInt();
}

BG96RegStatus AsyncGSM::getRegistrationStatus() {
  // Check first for EPS registration (preferred for LTE)
  BG96RegStatus epsStatus = (BG96RegStatus)getRegistrationStatusXREG("CEREG");

  // If we're connected on EPS, great!
  if (epsStatus == REG_OK_HOME || epsStatus == REG_OK_ROAMING) {
    return epsStatus;
  } else {
    // Otherwise, check generic network status
    return (BG96RegStatus)getRegistrationStatusXREG("CREG");
  }
}

bool AsyncGSM::isConnected() {
  BG96RegStatus s = getRegistrationStatus();
  return (s == REG_OK_HOME || s == REG_OK_ROAMING);
}

bool AsyncGSM::gprsDisconnect() {
  // Deactivate the GPRS context
  bool ok = at.sendCommand("AT+QIDEACT=1", "OK", 40000); // BG96 needs up to 40s
  return ok;
}

bool AsyncGSM::gprsConnect(const char *apn, const char *user, const char *pwd) {
  gprsDisconnect();

  // Configure PDP context - use context 1 for PDP, socket uses context 0
  String response;
  if (!at.sendCommand(response, "OK", 5000, "AT+QICSGP=1,1,\"", apn, "\",\"",
                      (user && *user) ? user : "", "\",\"",
                      (pwd && *pwd) ? pwd : "", "\"")) {
    log_e("Failed to configure PDP context");
    return false;
  }

  // Activate PDP context - this can take up to 150 seconds
  if (!at.sendCommand("AT+QIACT=1", "OK", 150000)) {
    log_e("Failed to activate PDP context");
    return false;
  }

  // Verify PDP context activation and get IP
  if (!at.sendCommand("AT+QIACT?", response, "+QIACT:", 5000)) {
    log_e("Failed to query PDP context");
    return false;
  }

  // Check if we got an IP address
  if (response.indexOf("+QIACT: 1,1,1") == -1) {
    log_e("PDP context not properly activated");
    return false;
  }

  // Attach to GPRS
  if (!at.sendCommand("AT+CGATT=1", "OK", 60000)) {
    log_e("Failed to attach to GPRS");
    return false;
  }

  log_i("GPRS connection established successfully");
  return true;
}

String AsyncGSM::getSimCCID() {
  String response;
  if (!at.sendCommand("AT+QCCID", response, "+QCCID:", 2000)) {
    return "";
  }

  int idx = response.indexOf("+QCCID:");
  if (idx == -1) return "";

  String rest = response.substring(idx);
  int colon = rest.indexOf(':');
  if (colon == -1) return "";

  String ccid = rest.substring(colon + 1);
  int end = ccid.indexOf("\r\n");
  if (end != -1) ccid = ccid.substring(0, end);

  ccid.trim();
  return ccid;
}

String AsyncGSM::getIMEI() {
  String response;
  if (!at.sendCommand("AT+CGSN", response, "OK", 2000)) {
    return "";
  }

  int start = response.indexOf("\r\n");
  if (start == -1) return "";

  String rest = response.substring(start + 2);
  int end = rest.indexOf("\r\n");
  if (end != -1) rest = rest.substring(0, end);

  rest.trim();
  return rest;
}

String AsyncGSM::getOperator() {
  String response;
  if (!at.sendCommand("AT+COPS?", response, "+COPS:", 2000)) {
    return "";
  }

  int idx = response.indexOf("+COPS:");
  if (idx == -1) return "";

  String rest = response.substring(idx);
  int firstQuote = rest.indexOf('"');
  if (firstQuote == -1) return "";

  String sub = rest.substring(firstQuote + 1);
  int secondQuote = sub.indexOf('"');
  if (secondQuote == -1) return "";

  String op = sub.substring(0, secondQuote);
  op.trim();
  return op;
}

String AsyncGSM::getIPAddress() {
  String response;
  if (!at.sendCommand("AT+QIACT?", response, "+QIACT:", 2000)) {
    return "";
  }

  // Parse: +QIACT: 1,1,1,"192.168.1.100"
  int idx = response.indexOf("+QIACT:");
  if (idx == -1) return "";

  String rest = response.substring(idx);

  // Find the IP address in quotes
  int firstQuote = rest.indexOf('"');
  if (firstQuote == -1) return "";

  String ipPart = rest.substring(firstQuote + 1);
  int secondQuote = ipPart.indexOf('"');
  if (secondQuote == -1) return "";

  String ip = ipPart.substring(0, secondQuote);
  ip.trim();
  return ip;
}

bool AsyncGSM::modemConnect(const char *host, uint16_t port) {
  String response;
  String portStr(port);

  log_d("Attempting to connect to %s:%d", host, port);

  // Send QIOPEN command - use context 1, connection ID 0
  if (!at.sendCommand(response, "OK", 15000, "AT+QIOPEN=1,0,\"TCP\",\"", host,
                      "\",", portStr.c_str(), ",0,0")) {
    log_e("Failed to send QIOPEN command");
    return false;
  }

  log_d("QIOPEN command accepted, waiting for connection URC");

  // Now wait for the URC response: +QIOPEN: <connectID>,<err>
  if (at.waitResponse(15000, "+QIOPEN:") == 0) {
    log_e("No +QIOPEN URC received");
    return false;
  }

  String urcResponse = at.getResponse("+QIOPEN:");
  log_d("Got URC: %s", urcResponse.c_str());

  // Parse the URC response: +QIOPEN: 0,0 (connectID, error)
  int idx = urcResponse.indexOf("+QIOPEN:");
  if (idx == -1) {
    log_e("Invalid URC format");
    return false;
  }

  String rest = urcResponse.substring(idx + 8); // Skip "+QIOPEN:"
  rest.trim();

  // Parse: 0,0 or 1,0,0 (context might be included)
  int firstComma = rest.indexOf(',');
  if (firstComma == -1) {
    log_e("Invalid URC format - no comma");
    return false;
  }

  // Get the error code (last number)
  String errorPart = rest.substring(firstComma + 1);
  int secondComma = errorPart.indexOf(',');
  if (secondComma != -1) {
    errorPart = errorPart.substring(secondComma + 1);
  }
  errorPart.trim();

  int errorCode = errorPart.toInt();
  bool success = (errorCode == 0);

  log_d("Connection result: %s (error code: %d)",
        success ? "SUCCESS" : "FAILED", errorCode);

  if (success) {
    // Clear any existing buffer data for fresh connection
    rxBuffer = "";
    endOfDataReached = false;
    consecutiveEmptyReads = 0;
  }

  return success;
}
