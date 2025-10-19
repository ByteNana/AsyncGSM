#include "GSMContext.h"

#include "esp_log.h"

GSMContext::GSMContext() { rxMutex = xSemaphoreCreateMutex(); }

bool GSMContext::begin(Stream &stream) {
  ioStream = &stream;
  if (!atHandler.begin(stream)) {
    log_e("Failed to initialize AsyncATHandler");
    return false;
  }
  rxTransport.init(stream, rxMutex);
  modemDriver.init(stream, atHandler, rxTransport);
  return true;
}

bool GSMContext::setupNetwork(const char *apn) {
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

  if (!modemDriver.setEchoOff()) return false;
  if (!modemDriver.enableVerboseErrors()) return false;
  if (!modemDriver.checkModemModel()) return false;
  if (!modemDriver.checkTimezone()) return false;

  while (true) {
    log_w("Waiting for SIM card...");
    if (modemDriver.checkSIMReady()) {
      log_d("SIM card is ready.");
      break;
    }
    delay(1000);
  }

  modemDriver.disableConnections();
  modemDriver.disalbeSleepMode();

  if (!modemDriver.gprsConnect(apn)) return false;

  for (int i = 0; i < 10; ++i) {
    if (modemDriver.isGPRSSAttached() && modemDriver.checkNetworkContext()) {
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
