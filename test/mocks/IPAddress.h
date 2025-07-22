#pragma once

#include <WString.h>
#include <cstdint>

class IPAddress {
public:
  IPAddress() : _address{0, 0, 0, 0} {}
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
      : _address{a, b, c, d} {}
  IPAddress(uint32_t addr) {
    _address[0] = (addr >> 24) & 0xFF;
    _address[1] = (addr >> 16) & 0xFF;
    _address[2] = (addr >> 8) & 0xFF;
    _address[3] = addr & 0xFF;
  }

  uint8_t operator[](int index) const { return _address[index]; }

  String toString() const {
    String s;
    s.reserve(16);
    s += _address[0];
    s += ".";
    s += _address[1];
    s += ".";
    s += _address[2];
    s += ".";
    s += _address[3];
    return s;
  }

private:
  uint8_t *raw_address() { return _address; }
  uint8_t _address[4];
};
