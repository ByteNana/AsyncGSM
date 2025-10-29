# AsyncGSM

AsyncGSM provides an asynchronous interface for controlling a GSM modem using AT commands.
It targets ESP32 devices and builds on top of [AsyncATHandler](https://github.com/ByteNana/AsyncATHandler).

## Building

This project uses `make` wrappers around CMake. To compile the library run:

```sh
make build
```

To execute the unit tests run:

```sh
make test
```

### Verbose Serial Logs (LOG_LEVEL=5)

To see the mock Serial TX/RX interaction during native tests, build and run with `LOG_LEVEL=5`.

- Using the Makefile (recommended):
  - `make test 5`
    - This configures CMake with `-DLOG_LEVEL=5`, builds, and runs tests.
  - Alternatively: `make build 5 && make test 5`

- Using CMake directly:
  - `cmake -S . -B build -DLOG_LEVEL=5`
  - `cmake --build build`
  - `ctest --output-on-failure --test-dir build`

When enabled, tests print serial traces like:

```
[SERIAL TX] AT+QIOPEN=1,0,"TCP","example.com",80,0,0
[SERIAL RX] OK
[SERIAL RX] +QIOPEN: 0,0
[SERIAL TX] GET / HTTP/1.1
[SERIAL TX] Host: example.com
[SERIAL RX] >
[SERIAL RX] OK
[SERIAL RX] SEND OK
```

## Example

A minimal sketch can be found in `examples/gsm`.
An HTTP client demonstration is available in `examples/http_client`.

For additional PlatformIO usage see the example directories.

## Hardware Tests

Hardware integration tests can be executed using PlatformIO. First ensure
PlatformIO is installed and an ESP32 board is connected. Then run:

```sh
make esp32-test
```

This command flashes the hardware test suites for both examples onto the
device and runs them using PlatformIO.
