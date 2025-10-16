#include "AsyncSecureGSM.h"
#include "esp_log.h"
#include <MD5Builder.h>

static String caMd5;

bool AsyncSecureGSM::modemConnect(const char *host, uint16_t port) {
  return MODEM().connectSecure(host, port);
}

bool AsyncSecureGSM::modemStop() { return MODEM().stopSecure(); }

void AsyncSecureGSM::setCACert(const char *rootCA) {
  if (!rootCA || !*rootCA) {
    return;
  }

  MD5Builder md5;
  md5.begin();
  md5.add((uint8_t *)rootCA, strlen(rootCA));
  md5.calculate();
  String newMd5 = md5.toString();

  if (newMd5 == caMd5) {
    return;
  }

  log_d("Setting CA cert with MD5 %s", newMd5.c_str());
  const char *filename = newMd5.c_str();
  String foundName;
  size_t foundSize = 0;
  bool exists = MODEM().findUFSFile(filename, &foundName, &foundSize);
  if (!exists) {
    log_d("Uploading CA cert to UFS as %s", filename);
    MODEM().uploadUFSFile(filename, reinterpret_cast<const uint8_t *>(rootCA), strlen(rootCA));
  }
  log_i("Configuring CA cert for SSL");
  MODEM().setCACertificate(filename);
  caMd5 = newMd5;
}
