#pragma once

#include <atomic>

enum class RegStatus {
  REG_NO_RESULT = -1,
  REG_UNREGISTERED = 0,
  REG_SEARCHING = 2,
  REG_DENIED = 3,
  REG_OK_HOME = 1,
  REG_OK_ROAMING = 5,
  REG_UNKNOWN = 4,
};

enum class ConnectionStatus {
  DISCONNECTED,
  FAILED,
  CONNECTED,
  CLOSING,
};

enum class MqttConnectionState {
  IDLE,
  DISCONNECTED,
  CONNECTED,
};

struct UrcState {
  std::atomic<RegStatus> creg{RegStatus::REG_NO_RESULT};
  std::atomic<ConnectionStatus> isConnected{ConnectionStatus::DISCONNECTED};
  std::atomic<MqttConnectionState> mqttState{MqttConnectionState::IDLE};
};
