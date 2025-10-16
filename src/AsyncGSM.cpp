#include "AsyncGSM.h"
#include "GSMContext/GSMContext.h"
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
  log_d("Initializing AsyncGSM...");
  if (!at.begin(stream)) {
    log_e("Failed to initialize AsyncATHandler");
    return false;
  }
  transport.init(stream, rxMutex);
  transport.setDefaultSSL(ssl);
  modem.init(stream, at, transport);

  return true;
}

// Context-aware accessors
AsyncATHandler &AsyncGSM::AT() { return ctx ? ctx->at() : at; }
AsyncEG915U &AsyncGSM::MODEM() { return ctx ? ctx->modem() : modem; }
GSMTransport &AsyncGSM::TRANSPORT() { return ctx ? ctx->transport() : transport; }

RegStatus AsyncGSM::getRegistrationStatus() {
  return modem.URCState.creg.load();
}

bool AsyncGSM::begin(const char *apn) {
  bool canCommunicate = false;
  for (int i = 0; i < 4; i++) {
    if (AT().sendSync("AT", 2000)) {
      canCommunicate = true;
      break;
    }
  }
  if (!canCommunicate) {
    log_e("Failed to communicate with modem");
    return false;
  }

  if (!MODEM().setEchoOff()) {
    return false;
  }

  if (!MODEM().enableVerboseErrors()) {
    return false;
  }

  if (!MODEM().checkModemModel()) {
    return false;
  }

  if (!MODEM().checkTimezone()) {
    return false;
  }

  String response;

  while (true) {
    log_w("Waiting for SIM card...");
    if (MODEM().checkSIMReady()) {
      log_d("SIM card is ready.");
      break;
    }
    delay(1000);
  }

  // Deactivate all contexts just in case
  MODEM().disableConnections();

  MODEM().disalbeSleepMode();

  if (!MODEM().gprsConnect(apn)) {
    return false;
  }

  for (int i = 0; i < 10; ++i) {
    if (MODEM().isGPRSSAttached() && MODEM().checkNetworkContext()) {
      log_d("GPRS is attached.");
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
  modemStop();
  TRANSPORT().reset();
  log_d("Connection stopped.");
}

size_t AsyncGSM::write(uint8_t c) { return write(&c, 1); }

size_t AsyncGSM::write(const uint8_t *buf, size_t size) {
  if (MODEM().URCState.isConnected.load() != ConnectionStatus::CONNECTED ||
      !AT().getStream()) {
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

  ATPromise *promise = AT().sendCommand(command + String(size));
  if (!promise) {
    log_e("Failed to create promise for AT+QISEND");
    AT().popCompletedPromise(promise->getId());
    return 0;
  }

  while (!AT().getStream()->available()) {
    vTaskDelay(0);
  }
  String response;
  while (AT().getStream()->available()) {
    char c = AT().getStream()->read();
    response += c;
    if (c == '>') {
      break;
    }
  }
  if (response.indexOf('>') == -1) {
    log_e("Did not receive prompt '>'");
    AT().popCompletedPromise(promise->getId());
    return 0;
  }

  AT().getStream()->write(buf, size);
  AT().getStream()->flush();

  bool promptReceived = promise->expect("SEND OK")->wait();

  auto p = AT().popCompletedPromise(promise->getId());
  if (!promptReceived) {
    log_e("Failed to get SEND OK confirmation");
    return 0;
  }

  log_d("Write successful.");
  return size;
}

int AsyncGSM::available() { return static_cast<int>(TRANSPORT().available()); }

int AsyncGSM::read() { return TRANSPORT().read(); }

int AsyncGSM::read(uint8_t *buf, size_t size) { return static_cast<int>(TRANSPORT().read(buf, size)); }

int AsyncGSM::peek() { return TRANSPORT().peek(); }

void AsyncGSM::flush() {
  log_w("Flushing stream...");
  if (!AT().getStream()) {
    log_e("Stream not initialized");
    return;
  }
  TRANSPORT().flush();
}

uint8_t AsyncGSM::connected() { return MODEM().URCState.isConnected.load() == ConnectionStatus::CONNECTED; }
