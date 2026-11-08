#pragma once

#include "../../core/ports/IHardwareIdentity.h"

class Esp8266ArduinoHardwareIdentity : public IHardwareIdentity {
 public:
  void readHardwareId(char* buffer, size_t bufferSize) const override;
};
