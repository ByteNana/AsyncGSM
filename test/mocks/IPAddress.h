#pragma once

#include <cstdint>

class IPAddress {
public:
    IPAddress() : _address{0, 0, 0, 0} {}
    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : _address{a, b, c, d} {}
    IPAddress(uint32_t addr) {
        _address[0] = (addr >> 24) & 0xFF;
        _address[1] = (addr >> 16) & 0xFF;
        _address[2] = (addr >> 8) & 0xFF;
        _address[3] = addr & 0xFF;
    }

    uint8_t* raw_address() { return _address; }

private:
    uint8_t _address[4];
};
