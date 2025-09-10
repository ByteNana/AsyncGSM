#pragma once

#include <Arduino.h>
#include <AsyncATHandler.h>
#include <Stream.h>

#include "EG915.settings.h"

class AsyncEG915U {
private:
  Stream *_stream = nullptr;
  AsyncATHandler *at;
  std::deque<uint8_t> *rxBuffer;

  void handleURC(const String &urc);

public:
  UrcState URCState;
  std::deque<uint8_t> *mqttRxBuffer = nullptr;

  AsyncEG915U();
  ~AsyncEG915U();
  bool init(Stream &stream, AsyncATHandler &atHandler,
            std::deque<uint8_t> &rxBuf);
  bool setEchoOff();
  bool enableVerboseErrors();
  bool checkModemModel();
  bool checkTimezone();
  bool checkSIMReady();
  void disablePDPContext();
  bool disalbeSleepMode();
  bool checkGPRSSAttached();
  bool gprsConnect(const char *apn, const char *user = nullptr,
                   const char *pass = nullptr);
  bool gprsDisconnect();
  bool checkPDPContext();
  bool attachGPRS();
  String getSimCCID();
  String getIMEI();
  String getOperator();
  String getIPAddress();
  bool connect(const char *host, uint16_t port);
  bool connectSecure(const char *host, uint16_t port);
};
