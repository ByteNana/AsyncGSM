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

For additional PlatformIO usage see the `examples/gsm` directory.
