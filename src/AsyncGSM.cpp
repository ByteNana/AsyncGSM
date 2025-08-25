#include "AsyncGSM.h"
#include "esp_log.h"
#include <algorithm>
#include <limits>

AsyncGSM::AsyncGSM() : _connected(false) {}

AsyncGSM::~AsyncGSM() {
  if (at.getStream()) {
    at.end();
  }
}

bool AsyncGSM::init(Stream &stream) {
  _connected = false;
  log_i("Initializing AsyncGSM...");
  if (!at.begin(stream)) {
    log_e("Failed to initialize AsyncATHandler");
    return false;
  }
  modem.init(stream, at, rxBuffer);

  return true;
}

RegStatus AsyncGSM::getRegistrationStatus() { return modem.URCState.creg; }

bool AsyncGSM::begin(const char *apn) {
  bool canCommunicate = false;
  for (int i = 0; i < 4; i++) {
    if (at.sendSync("AT", 2000)) {
      canCommunicate = true;
      break;
    }
  }
  if (!canCommunicate) {
    log_e("Failed to communicate with modem");
    return false;
  }

  if (!modem.setEchoOff()) {
    return false;
  }

  if (!modem.enableVerboseErrors()) {
    return false;
  }

  if (!modem.checkModemModel()) {
    return false;
  }

  if (!modem.checkTimezone()) {
    return false;
  }

  String response;

  while (true) {
    log_w("Waiting for SIM card...");
    if (modem.checkSIMReady()) {
      log_i("SIM card is ready.");
      break;
    }
    delay(1000);
  }

  // Deactivate all contexts just in case
  modem.disablePDPContext();

  modem.disalbeSleepMode();

  if (!modem.gprsConnect(apn)) {
    return false;
  }

  for (int i = 0; i < 10; ++i) {
    if (modem.checkGPRSSAttached()) {
      log_i("GPRS is attached.");
      return true;
    }
    delay(500);
  }
  return false;
}

int AsyncGSM::connect(IPAddress ip, uint16_t port) {
  return connect(ip.toString().c_str(), port);
}

int AsyncGSM::connect(const char *host, uint16_t port) {
  log_i("Connecting to %s:%d", host, port);
  stop();
  _connected = modemConnect(host, port);
  return _connected ? 1 : 0;
}

void AsyncGSM::stop() {
  log_i("Stopping connection...");
  _connected = false;
  if (!at.sendSync("AT+QICLOSE=0", 4000)) {
    log_w("Failed to close connection");
  }
  if (!at.sendSync("AT+QIDEACT=1", 4000)) {
    log_w("PDP context deactivation failed or already inactive");
  }
}

size_t AsyncGSM::write(uint8_t c) { return write(&c, 1); }

size_t AsyncGSM::write(const uint8_t *buf, size_t size) {
  if (!_connected || !at.getStream()) {
    log_e("Not connected or stream not initialized");
    return 0;
  }

  if (size == 0)
    return 0;
  log_d("Writing %zu bytes to modem...", size);

  String out;
  out.reserve(size);
  for (size_t i = 0; i < size; i++) {
    out += char(buf[i]); // convert each byte to char
  }
  log_d("Sent bytes: %s", out.c_str());

  ATPromise *promise = at.sendCommand(String("AT+QISEND=0,") + String(size));
  if (!promise) {
    log_e("Failed to create promise for AT+QISEND");
    at.popCompletedPromise(promise->getId());
    return 0;
  }

  bool promptReceived = promise->expect(">")->wait();
  auto p = at.popCompletedPromise(promise->getId());
  if (!promptReceived) {
    log_e("Did not receive prompt '>'");
    return 0;
  }

  at.getStream()->write(buf, size);
  at.getStream()->flush();

  ATPromise *dataPromise = at.sendCommand("");
  if (!dataPromise) {
    log_e("Failed to create promise for final response");
    at.popCompletedPromise(dataPromise->getId());
    return 0;
  }

  bool sendOkReceived = dataPromise->expect("SEND OK")->wait();
  auto p2 = at.popCompletedPromise(dataPromise->getId());

  if (!sendOkReceived) {
    log_e("Failed to get SEND OK confirmation");
    return 0;
  }

  log_d("Write successful.");
  return size;
}

int AsyncGSM::available() {
  log_d("Checking available bytes in buffer...");
#ifdef ON_UNIT_TESTS
  const char *cmd = "AT+QIRD\r\n";
  at.getStream()->print(cmd);
  at.getStream()->print("\r\n");
  at.getStream()->flush();
  vTaskDelay(pdMS_TO_TICKS(100));
#endif
  return rxBuffer.length();
}

int AsyncGSM::read() {
  if (rxBuffer.length() == 0) {
    return -1;
  }

  char c = rxBuffer.charAt(0);
  rxBuffer = rxBuffer.substring(1);
  return c;
}

int AsyncGSM::read(uint8_t *buf, size_t size) {
  log_d("Reading up to %zu bytes from buffer...", size);
  if (rxBuffer.length() == 0 || size == 0) {
    return 0;
  }

  size_t toCopy = min(size, (size_t)rxBuffer.length());
  memcpy(buf, rxBuffer.c_str(), toCopy);
  rxBuffer = rxBuffer.substring(toCopy);
  return toCopy;
}

int AsyncGSM::peek() {
  log_d("Peeking into buffer...");
  if (rxBuffer.length() == 0) {
    return -1;
  }
  return rxBuffer.charAt(0);
}

void AsyncGSM::flush() {
  log_w("Flushing stream...");
  if (!at.getStream()) {
    log_e("Stream not initialized");
    return;
  }
  at.getStream()->flush();
}

uint8_t AsyncGSM::connected() { return _connected; }
