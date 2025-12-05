#pragma once

#include <Arduino.h>
#include <AsyncATHandler.h>
#include <Stream.h>
#include <utils/MqttQueue/MqttQueue.h>

#include "Modem.settings.h"
#include "SIMCard/SIMCard.h"
#include "SIMCard/SIMCard.settings.h"

class GSMTransport;

class ModemBackend {
 private:
  Stream *_stream = nullptr;
  GSMTransport *transport = nullptr;
  AsyncATHandler *at = nullptr;
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
  UrcState URCState;
  AtomicMqttQueue *mqttQueueSub = nullptr;
  ModemSIMCard sim;

  ModemBackend() = default;
  virtual ~ModemBackend() = 0;

  virtual bool init(Stream &stream, AsyncATHandler &atHandler, GSMTransport &transport);
  virtual bool setEchoOff();
  virtual bool enableVerboseErrors();
  virtual bool checkModemModel();
  virtual bool checkTimezone();
  virtual bool checkSIMReady();
  virtual bool disalbeSleepMode();
  virtual bool gprsConnect(const char *apn, const char *user = nullptr, const char *pass = nullptr);
  virtual bool gprsDisconnect();
  virtual String getSimCCID();
  virtual String getIMEI();
  virtual String getIMSI();
  virtual String getOperator();
  virtual String getIPAddress();
  virtual bool connect(const char *host, uint16_t port);
  virtual bool stop();
  virtual bool connectSecure(const char *host, uint16_t port);
  virtual bool connectSecure(const char *host, uint16_t port, const char *caCertPath);
  virtual bool stopSecure();
  virtual bool setSIMSlot(SimSlot slot);

  virtual bool uploadUFSFile(
      const char *path, const uint8_t *data, size_t size, uint32_t timeoutMs = 120000);
  virtual bool setCACertificate(const char *ufsPath, const char *ssl_cidx);
  virtual bool findUFSFile(
      const char *pattern, String *outName = nullptr, size_t *outSize = nullptr);

  // Helpers for GPRS connection
  virtual void disableConnections();
  virtual bool setPDPContext(const char *apn);
  virtual bool activatePDPContext();
  virtual bool isGPRSSAttached();
  virtual bool checkNetworkContext();
  virtual bool activatePDP();
  virtual bool attachGPRS();
};

// Provide definition for pure-virtual destructor
inline ModemBackend::~ModemBackend() {};
