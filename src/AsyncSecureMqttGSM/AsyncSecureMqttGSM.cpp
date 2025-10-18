#include "AsyncSecureMqttGSM.h"

#include <MD5Builder.h>

#include "esp_log.h"

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
  // Our modem driver currently targets SSL context 1 for cacert
  context().modem().setCACertificate(filename, cidx);
  caMd5Cache = newMd5;
}
