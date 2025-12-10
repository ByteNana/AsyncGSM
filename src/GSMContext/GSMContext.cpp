#include "GSMContext.h"

#include "esp_log.h"

GSMContext::GSMContext() { rxMutex = xSemaphoreCreateMutex(); }

bool GSMContext::begin(Stream &stream, iGSMModule &module, SIMSlot simSlot) {
  this->simSlot = simSlot;
  this->modemDriver = &module;
  ioStream = &stream;
  if (!atHandler.begin(stream)) {
    log_e("Failed to initialize AsyncATHandler");
    return false;
  }
  rxTransport.init(stream, rxMutex);
  modemDriver->init(stream, atHandler, rxTransport);
  return true;
}

bool GSMContext::setupNetwork(const char *apn) {
  if (!modemDriver) {
    log_e("Modem driver not initialized");
    return false;
  }

  bool canCommunicate = false;
  for (int i = 0; i < 4; i++) {
    if (atHandler.sendSync("AT", 2000)) {
      canCommunicate = true;
      break;
    }
  }
  if (!canCommunicate) {
    log_e("Failed to communicate with modem");
    return false;
  }

  if (!modemDriver->setEchoOff()) return false;
  if (!modemDriver->enableVerboseErrors()) return false;
  if (!modemDriver->checkModemModel()) return false;
  if (!modemDriver->checkTimezone()) return false;
  if (!modemDriver->sim().setSlot(simSlot)) return false;

  bool simReady = false;
  uint8_t retries = 0;
  do {
    log_w("Waiting for SIM card...");
    delay(1000);
    simReady = modemDriver->checkSIMReady();
    retries++;
  } while (!simReady && retries < 10);

  if (!simReady) {
    log_e("SIM card not ready");
    return false;
  }

  log_d("SIM card is ready.");

  modemDriver->disableConnections();
  modemDriver->disalbeSleepMode();

  if (!modemDriver->network().gprsConnect(apn)) return false;

  for (int i = 0; i < 10; ++i) {
    if (modemDriver->network().isGPRSSAttached() && modemDriver->network().checkContext()) {
      log_d("GPRS is attached.");
      return true;
    }
    delay(500);
  }
  return false;
}

void GSMContext::end() {
  if (ioStream) {
    atHandler.end();
    // Give the handler a brief window to fully stop internal tasks
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  ioStream = nullptr;
}