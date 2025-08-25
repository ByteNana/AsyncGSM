#pragma once

enum RegStatus {
  REG_NO_RESULT = -1,
  REG_UNREGISTERED = 0,
  REG_SEARCHING = 2,
  REG_DENIED = 3,
  REG_OK_HOME = 1,
  REG_OK_ROAMING = 5,
  REG_UNKNOWN = 4,
};

struct UrcState {
  volatile RegStatus creg = REG_NO_RESULT;
  volatile int isConnected = 0;
};
