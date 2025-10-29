#include "AsyncSecureMqttGSM.h"

#include <MD5Builder.h>

#include "esp_log.h"

void AsyncSecureMqttGSM::setSecurityLevel(bool secure) {
  auto &at = context().at();

  String secureLevel = (isSecure() ? "1" : "0");
  // Enable SSL for the MQTT client and bind to SSL context index
  if (!at.sendSync(String("AT+QMTCFG=\"ssl\",") + cidx + "," + secureLevel + "," + ssl_cidx)) {
    log_e("Failed to set SSL for MQTT");
  }
}

bool AsyncSecureMqttGSM::connect(const char *id, const char *user, const char *pass) {
  setSecurityLevel(isSecure());
  return AsyncMqttGSM::connect(id, user, pass);
}

bool AsyncSecureMqttGSM::publish(const char *topic, const uint8_t *payload, unsigned int plength) {
  setSecurityLevel(isSecure());
  return AsyncMqttGSM::publish(topic, payload, plength);
}

bool AsyncSecureMqttGSM::subscribe(const char *topic) {
  setSecurityLevel(isSecure());
  return AsyncMqttGSM::subscribe(topic, 0);
}

void AsyncSecureMqttGSM::setCACert(const char *rootCA) {
  if (!rootCA || !*rootCA) { return; }

  MD5Builder md5;
  md5.begin();
  md5.add((uint8_t *)rootCA, strlen(rootCA));
  md5.calculate();
  String newMd5 = md5.toString();
  if (newMd5 == caMd5Cache) { return; }

  const char *filename = newMd5.c_str();
  String found;
  size_t fsize = 0;
  bool exists = context().modem().findUFSFile(filename, &found, &fsize);
  if (!exists) {
    context().modem().uploadUFSFile(
        filename, reinterpret_cast<const uint8_t *>(rootCA), strlen(rootCA));
  }

  context().modem().setCACertificate(filename, ssl_cidx);
  caMd5Cache = newMd5;
}
