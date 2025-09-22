#include "AsyncGSM.h"
#include "esp_log.h"
#include <algorithm>
#include <limits>

AsyncGSM::AsyncGSM() {}

AsyncGSM::~AsyncGSM() {
  if (at.getStream()) {
    at.end();
  }
}

bool AsyncGSM::init(Stream &stream) {
  log_i("Initializing AsyncGSM...");
  if (!at.begin(stream)) {
    log_e("Failed to initialize AsyncATHandler");
    return false;
  }
  modem.init(stream, at, rxBuffer);

  return true;
}

RegStatus AsyncGSM::getRegistrationStatus() {
  return modem.URCState.creg.load();
}

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
  modem.disableConnections();

  modem.disalbeSleepMode();

  if (!modem.gprsConnect(apn)) {
    return false;
  }

  for (int i = 0; i < 10; ++i) {
    if (modem.isGPRSSAttached() && modem.checkNetworkContext()) {
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
  return modemConnect(host, port);
}

void AsyncGSM::stop() {
  log_i("Stopping connection...");
  modemStop();
  log_i("Connection stopped.");
}

size_t AsyncGSM::write(uint8_t c) { return write(&c, 1); }

size_t AsyncGSM::write(const uint8_t *buf, size_t size) {
  if (modem.URCState.isConnected.load() != ConnectionStatus::CONNECTED ||
      !at.getStream()) {
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

  String command = ssl ? "AT+QSSLSEND=0," : "AT+QISEND=0,";

  ATPromise *promise = at.sendCommand(command + String(size));
  if (!promise) {
    log_e("Failed to create promise for AT+QISEND");
    at.popCompletedPromise(promise->getId());
    return 0;
  }

  while (!at.getStream()->available()) {
    vTaskDelay(0);
  }
  String response;
  while (at.getStream()->available()) {
    char c = at.getStream()->read();
    response += c;
    if (c == '>') {
      break;
    }
  }
  if (response.indexOf('>') == -1) {
    log_e("Did not receive prompt '>'");
    at.popCompletedPromise(promise->getId());
    return 0;
  }

  at.getStream()->write(buf, size);
  at.getStream()->flush();

  bool promptReceived = promise->expect("SEND OK")->wait();

  auto p = at.popCompletedPromise(promise->getId());
  if (!promptReceived) {
    log_e("Failed to get SEND OK confirmation");
    return 0;
  }

  log_d("Write successful.");
  return size;
}

int AsyncGSM::available() {
  vTaskDelay(pdMS_TO_TICKS(10));

  return (int)rxBuffer.size();
}

int AsyncGSM::read() {
  // Avoid logs here.
  // log_d("read() called...");
  if (rxBuffer.size() == 0) {
    return -1;
  }
  char c = rxBuffer.front();
  rxBuffer.pop_front();
  return static_cast<unsigned char>(c);
}

int AsyncGSM::read(uint8_t *buf, size_t size) {
  log_d("Reading up to %zu bytes from buffer...", size);
  if (rxBuffer.empty() || size == 0) {
    return 0;
  }

  size_t toCopy = min(size, rxBuffer.size());
  for (size_t i = 0; i < toCopy; i++) {
    buf[i] = rxBuffer.front();
    rxBuffer.pop_front();
  }
  return toCopy;
}

int AsyncGSM::peek() {
  log_d("Peeking into buffer...");
  if (rxBuffer.size() == 0) {
    return -1;
  }
  return rxBuffer.front();
}

void AsyncGSM::flush() {
  log_w("Flushing stream...");
  if (!at.getStream()) {
    log_e("Stream not initialized");
    return;
  }
  at.getStream()->flush();
}

uint8_t AsyncGSM::connected() {
  return modem.URCState.isConnected.load() == ConnectionStatus::CONNECTED;
}
