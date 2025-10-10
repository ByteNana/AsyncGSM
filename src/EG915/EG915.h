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
  SemaphoreHandle_t rxMutex = nullptr;

  // Dynamic URC registration helpers
  void registerURCs();
  void unregisterURCs();
  std::vector<String> registeredURCPatterns;

  // Per-URC callbacks
  void onRegChanged(const String &urc);
  void onOpenResult(const String &urc);
  void onClosed(const String &urc);
  void onTcpRecv(const String &urc);
  void onSslRecv(const String &urc);
  void onReadData(const String &urc);
  void onMqttRecv(const String &urc);
  void onMqttStat(const String &urc);
  void lockRx() { xSemaphoreTake(rxMutex, portMAX_DELAY); }
  void unlockRx() { xSemaphoreGive(rxMutex); }

public:
  UrcState URCState;
  AtomicMqttQueue *mqttQueueSub = nullptr;

  AsyncEG915U();
  ~AsyncEG915U();
  bool init(Stream &stream, AsyncATHandler &atHandler,
            std::deque<uint8_t> &rxBuf, SemaphoreHandle_t &mutex);
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
