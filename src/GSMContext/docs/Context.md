# GSMContext Usage

This guide shows how to run two long‑living clients (plain HTTP and HTTPS) over a single modem/AT/transport stack using `GSMContext`.

## Overview

- `GSMContext`: Owns the shared stack once (`AsyncATHandler`, `GSMTransport`, `AsyncEG915U`).
- `AsyncGSM`: Plain TCP client that can attach to a `GSMContext`.
- `AsyncSecureGSM`: TLS client that can attach to the same `GSMContext` and manage CA certificates.
- Benefit: Initialize hardware once, keep both clients alive, switch by protocol.

## Initialize Once

```
#include <GSMContext.h>
#include <AsyncGSM.h>
#include <AsyncSecureGSM.h>

GSMContext ctx;
AsyncGSM httpClient;         // plain HTTP
AsyncSecureGSM httpsClient;  // HTTPS (TLS)

void setup() {
  Serial.begin(115200);

  // 1) Bring up the shared context
  if (!ctx.begin(Serial)) {
    // handle error
  }
  if (!ctx.setupNetwork("your-apn")) {
    // handle error
  }

  // 2) Attach both clients to the same context
  httpClient.attach(ctx);
  httpsClient.attach(ctx);

  // 3) Configure CA for HTTPS (MD5‑named file on UFS)
  extern const char* ROOT_CA_PEM; // your PEM string
  httpsClient.setCACert(ROOT_CA_PEM);
}
```

## Using The Clients

- Only one socket should be active at a time.
- Stop the active client before switching to the other.

```
// HTTP
if (httpClient.connect("example.com", 80)) {
  httpClient.write((const uint8_t*)"GET / HTTP/1.1\r\nHost: example.com\r\n\r\n", 44);
  // read with httpClient.read()/available()
  httpClient.stop();
}

// HTTPS
if (httpsClient.connect("example.com", 443)) {
  httpsClient.write((const uint8_t*)"GET / HTTP/1.1\r\nHost: example.com\r\n\r\n", 44);
  // read with httpsClient.read()/available()
  httpsClient.stop();
}
```

## Certificate Handling (HTTPS)

- `AsyncSecureGSM::setCACert(const char* pem)`:
  - Computes MD5 of `pem` and uses the 32‑hex digest as the UFS filename.
  - If the file exists (via `QFLST`) it reuses it; otherwise uploads (`QFUPL`).
  - Binds the SSL context to that file with `QSSLCFG="cacert"` and sets `seclevel=1`.
  - Caches the MD5 to avoid rework when unchanged.

## API Reference

- `bool GSMContext::begin(Stream& stream)`
- `bool GSMContext::setupNetwork(const char* apn)`
- `void GSMContext::end()`
- `bool AsyncGSM::attach(GSMContext& ctx)`
- `bool AsyncSecureGSM::attach(GSMContext& ctx)` (via AsyncGSM base)
- `void AsyncSecureGSM::setCACert(const char* pem)`
- `int AsyncGSM::connect(const char* host, uint16_t port)` / `stop()` / `read()`
- `int AsyncSecureGSM::connect(const char* host, uint16_t port)` / `stop()` / `read()`

## Notes

- Time sync matters for TLS validation (`AT+CTZU=1` is enabled during setup).
- `GSMContext` mirrors the old `AsyncGSM::begin()` bring‑up flow, but centralizes ownership so multiple clients can share the stack safely.
- The same UFS helpers (list/upload) are reused by both HTTP(S) and MQTT(S).
