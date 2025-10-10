#pragma once

#include "IPAddress.h"
#include "Stream.h"
#include <stddef.h>
#include <stdint.h>

class Client : public Stream {
public:
  virtual ~Client() = default;

  virtual int connect(const char *host, uint16_t port) = 0;
  virtual int connect(IPAddress ip, uint16_t port) = 0;

  using Stream::write; // expose write overloads
  virtual void flush() = 0;

  virtual int available() = 0;
  virtual int read() = 0;
  virtual int read(uint8_t *buf, size_t size) = 0;
  virtual int peek() = 0;

  virtual void stop() = 0;
  virtual uint8_t connected() = 0;
  virtual operator bool() = 0;
};
