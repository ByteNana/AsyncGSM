#pragma once

#include <Arduino.h>
#include <AsyncATHandler.h>
#include <modules/iGSMModule/iSocket/iSocket.h>

class EG915Socket : public iSocketConnection {
 public:
  EG915Socket();

  void init(AsyncATHandler &handler);
  bool connect(const char *host, uint16_t port) override;
  bool connectSecure(const char *host, uint16_t port) override;
  bool connectSecure(const char *host, uint16_t port, const char *caCertPath = nullptr) override;
  bool stop() override;
  bool stopSecure() override;
  size_t sendData(const uint8_t *data, size_t size, bool isSecure) override;
  ConnectionStatus getConnectionStatus() override;
  bool uploadUFSFile(
      const char *path, const uint8_t *data, size_t size, uint32_t timeoutMs) override;
  bool setCACertificate(const char *ufsPath, const char *ssl_cidx) override;
  bool findUFSFile(
      const char *pattern, String *outName = nullptr, size_t *outSize = nullptr) override;

 private:
  AsyncATHandler *at = nullptr;
};
