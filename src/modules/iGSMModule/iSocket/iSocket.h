#pragma once

#include <Arduino.h>
#include <utils/GSMTransport/GSMTransport.h>

#include "iSocket.types.h"

class iSocketConnection {
 public:
  virtual ~iSocketConnection() = default;
  virtual bool connect(const char *host, uint16_t port) = 0;
  virtual bool stop() = 0;
  virtual size_t sendData(const uint8_t *data, size_t size, bool isSecure) = 0;
  virtual ConnectionStatus getConnectionStatus() = 0;

  // Secure connection methods
  virtual bool connectSecure(const char *host, uint16_t port) = 0;
  virtual bool connectSecure(const char *host, uint16_t port, const char *caCertPath) = 0;
  virtual bool stopSecure() = 0;

  // UFS file operations
  virtual bool uploadUFSFile(
      const char *path, const uint8_t *data, size_t size, uint32_t timeoutMs) = 0;
  virtual bool setCACertificate(const char *ufsPath, const char *ssl_cidx) = 0;
  virtual bool findUFSFile(
      const char *pattern, String *outName = nullptr, size_t *outSize = nullptr) = 0;

  inline void setIsConnected(ConnectionStatus status) {
    log_v("Socket connection status changed to %d", static_cast<int>(status));
    isConnected.store(status);
  }

 protected:
  bool certConfigured = false;
  GSMTransport *transport = nullptr;
  std::atomic<ConnectionStatus> isConnected = ConnectionStatus::DISCONNECTED;
};
