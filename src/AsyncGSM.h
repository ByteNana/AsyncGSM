#pragma once

#include <Arduino.h>
#include <Client.h>
#include <AsyncATHandler.h>

class AsyncGSM : Client {
  AsyncATHandler *at;
  uint8_t mux;
  uint16_t sock_available;
  uint32_t prev_check;
  bool sock_connected;
  bool got_data;
  // RxFifo rx;

  bool init(Stream *modem, uint8_t mux = 0);
};
