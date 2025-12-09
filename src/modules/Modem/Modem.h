#pragma once

#include "ModemBackend.h"

enum ModemType { EG915U, SIM7080 };

/* Modem is just an interface which on creation*/
class Modem {
 private:
  std::unique_ptr<ModemBackend> backend = nullptr;

 public:
  Modem(ModemType type) {
    switch (type) {
      case EG915U:
        backend = std::make_unique<EG915ModemBackend>();
        break;
      case SIM7080:
        // backend = new SIM7080ModemBackend();
        break;
    }
  }

  ModemBackend* operator->() { return backend.get(); }
};