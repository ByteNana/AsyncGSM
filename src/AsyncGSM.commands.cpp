#include "AsyncGSM.h"

bool AsyncGSM::gprsDisconnect() {
  // Deactivate the GPRS context
  String response;
  bool ok = at.sendCommand("AT+QIDEACT=1", response, "OK", 4000);
  return ok;
}

bool AsyncGSM::gprsConnect(const char *apn, const char *user, const char *pwd) {
  gprsDisconnect();

  // Configure the TCPIP Context
  // sendAT(GF("+QICSGP=1,1,\""), apn, GF("\",\""), user, GF("\",\""), pwd,
  //        GF("\""));
  // if (waitResponse() != 1) {
  //   return false;
  // }
  //
  // // Activate GPRS/CSD Context
  // sendAT(GF("+QIACT=1"));
  // if (waitResponse(150000L) != 1) {
  //   return false;
  // }
  //
  // // Attach to Packet Domain service - is this necessary?
  // sendAT(GF("+CGATT=1"));
  // if (waitResponse(60000L) != 1) {
  //   return false;
  // }

  return true;
}
