#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "IoTLabsTypes.h"

class IoTLabsStorage {
 public:
  bool begin(const char* ns);
  void end();

  bool hasWifi();
  String wifiSsid();
  String wifiPassword();
  bool saveWifi(const String& ssid, const String& password);
  bool clearWifi();

  String activationToken();
  bool saveActivationToken(const String& token);
  bool clearActivationToken();

  IoTLabsRuntimeIdentity runtimeIdentity();
  bool saveRuntimeIdentity(const IoTLabsRuntimeIdentity& identity);
  bool clearRuntimeIdentity();

  bool clearAll();

 private:
  Preferences _prefs;
  bool _opened = false;
};
