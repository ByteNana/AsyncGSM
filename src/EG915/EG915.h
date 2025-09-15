#pragma once

#include <Arduino.h>
#include <AsyncATHandler.h>
#include <MqttQueue/MqttQueue.h>
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
  AtomicMqttQueue *mqttQueueSub = nullptr;

  AsyncEG915U();
  ~AsyncEG915U();
  bool init(Stream &stream, AsyncATHandler &atHandler,
            std::deque<uint8_t> &rxBuf);
  bool setEchoOff();
  bool enableVerboseErrors();
  bool checkModemModel();
  bool checkTimezone();
  bool checkSIMReady();
  bool disalbeSleepMode();
  bool gprsConnect(const char *apn, const char *user = nullptr,
                   const char *pass = nullptr);
  bool gprsDisconnect();
  String getSimCCID();
  String getIMEI();
  String getOperator();
  String getIPAddress();
  bool connect(const char *host, uint16_t port);
  bool stop();
  bool connectSecure(const char *host, uint16_t port);
  bool stopSecure();

  // Helpers for GPRS connection
  void disableConnections();
  bool setPDPContext(const char *apn);
  bool activatePDPContext();
  bool isGPRSSAttached();
  bool checkNetworkContext();
  bool activatePDP();
  bool attachGPRS();
};
