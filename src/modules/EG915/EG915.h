#pragma once

#include <Arduino.h>
#include <AsyncATHandler.h>
#include <Stream.h>
#include <modules/iGSMModule/iGSMModule.h>

#include "EG915.settings.h"
#include "MQTT/MQTT.h"
#include "ModemInfo/ModemInfo.h"
#include "Network/Network.h"
#include "SIMCard/SIMCard.h"
#include "Socket/Socket.h"

class AsyncEG915U : public iGSMModule {
 private:
  Stream *_stream = nullptr;
  GSMTransport *transport = nullptr;
  AsyncATHandler *at;
  bool certConfigured = false;

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

 public:
  EG915SIMCard simModule;
  EG915Network networkModule;
  EG915ModemInfo modemInfoModule;
  EG915Socket socketModule;
  EG915MQTT mqttModule;

  AsyncEG915U();
  ~AsyncEG915U();

  // Interface implementations
  iSIMCard &sim() override { return simModule; }
  iGSMNetwork &network() override { return networkModule; }
  iModemInfo &modemInfo() override { return modemInfoModule; }
  iSocketConnection &socket() override { return socketModule; }
  iMQTT &mqtt() override { return mqttModule; }

  bool init(Stream &stream, AsyncATHandler &atHandler, GSMTransport &transport) override;

  bool setEchoOff() override;
  bool enableVerboseErrors() override;
  bool checkModemModel() override;
  bool checkTimezone() override;
  bool checkSIMReady() override;
  bool disalbeSleepMode() override;

  ConnectionStatus getConnectionStatus() override;

  // Helpers for GPRS connection
  void disableConnections() override;
};
