#include "AsyncGSM.h"

bool AsyncGSM::init(Stream *modem, uint8_t mux) {
  this->at = modem;
  sock_available = 0;
  prev_check = 0;
  sock_connected = false;
  got_data = false;

  // if (mux < TINY_GSM_MUX_COUNT) {
  //   this->mux = mux;
  // } else {
  //   this->mux = (mux % TINY_GSM_MUX_COUNT);
  // }
  at->sockets[this->mux] = this;

  return true;
}
