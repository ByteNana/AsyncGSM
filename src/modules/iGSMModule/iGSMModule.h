#pragma once

#include <Arduino.h>
#include <AsyncATHandler.h>

#include "iGSMModemInfo/iGSMModemInfo.h"
#include "iGSMNetwork/iGSMNetwork.h"
#include "iMQTT/iMQTT.h"
#include "iSIMCard/iSIMCard.h"
#include "iSocket/iSocket.h"

class iGSMModule {
 public:
  virtual ~iGSMModule() = default;
  virtual iSIMCard &sim() = 0;
  virtual iGSMNetwork &network() = 0;
  virtual iModemInfo &modemInfo() = 0;
  virtual iSocketConnection &socket() = 0;
  virtual iMQTT &mqtt() = 0;

  // Initialization
  virtual bool init(Stream &stream, AsyncATHandler &atHandler, GSMTransport &transport) = 0;
  virtual bool setEchoOff() = 0;
  virtual bool enableVerboseErrors() = 0;
  virtual bool checkModemModel() = 0;
  virtual bool checkTimezone() = 0;
  virtual bool checkSIMReady() = 0;
  virtual bool disalbeSleepMode() = 0;

  // State
  virtual ConnectionStatus getConnectionStatus() = 0;

  // Helpers for GPRS
  virtual void disableConnections() = 0;
};
