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
