#include "AsyncSecureGSM.h"

#include <MD5Builder.h>

#include "esp_log.h"

static String caMd5;

AsyncSecureGSM::AsyncSecureGSM(GSMContext &context) : AsyncGSM(context) {
  // Base ctor cannot see our override; enforce SSL default here
  ctx->transport().setDefaultSSL(true);
}

AsyncSecureGSM::AsyncSecureGSM() : AsyncGSM() {
  // Base ctor cannot see our override; enforce SSL default here
  ctx->transport().setDefaultSSL(true);
}

bool AsyncSecureGSM::modemConnect(const char *host, uint16_t port) {
  return ctx->modem().connectSecure(host, port);
}

bool AsyncSecureGSM::modemStop() { return ctx->modem().stopSecure(); }

void AsyncSecureGSM::setCACert(const char *rootCA) {
  if (!rootCA || !*rootCA) { return; }

  MD5Builder md5;
  md5.begin();
  md5.add((uint8_t *)rootCA, strlen(rootCA));
  md5.calculate();
  String newMd5 = md5.toString();

  if (newMd5 == caMd5) { return; }

  log_d("Setting CA cert with MD5 %s", newMd5.c_str());
  const char *filename = newMd5.c_str();
  String foundName;
  size_t foundSize = 0;
  bool exists = ctx->modem().findUFSFile(filename, &foundName, &foundSize);
  if (!exists) {
    log_d("Uploading CA cert to UFS as %s", filename);
    ctx->modem().uploadUFSFile(filename, reinterpret_cast<const uint8_t *>(rootCA), strlen(rootCA));
  }
  log_i("Configuring CA cert for SSL");
  ctx->modem().setCACertificate(filename);
  caMd5 = newMd5;
}
