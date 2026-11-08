#include "Stm32ArduinoStorage.h"

#if defined(ARDUINO_ARCH_STM32)

#include <EEPROM.h>

#include <cstring>

namespace {

constexpr uint32_t kMagic = 0x53544F49u;
constexpr uint8_t kLayoutVersion = 1;

constexpr uint16_t kMagicOffset = 0;
constexpr uint16_t kVersionOffset = 4;
constexpr uint16_t kFirstFieldOffset = 5;

struct FieldSlot {
  uint16_t offset;
  uint16_t capacity;  // payload bytes only, not counting the 2-byte length prefix.
};

// Order matches Stm32ArduinoStorage::Field. See the byte-layout table in
// Stm32ArduinoStorage.h for the full rationale/derivation of every offset.
constexpr FieldSlot kFields[] = {
    /*kWifiSsid*/ {5, 32},
    /*kWifiPass*/ {39, 64},
    /*kActToken*/ {105, 128},
    /*kDeviceId*/ {235, 64},
    /*kDeviceTemplateId*/ {301, 64},
    /*kMqttCid*/ {371, 64},
    /*kMqttUser*/ {437, 64},
    /*kMqttPass*/ {503, 128},
    /*kMqttHost*/ {633, 128},
    /*kTopicPref*/ {765, 64},
};

constexpr uint16_t kProfileVersionOffset = 367;
constexpr uint16_t kMqttPortOffset = 763;

// --- Raw buffered-EEPROM helpers. Callers are responsible for bracketing a
// sequence of these with exactly one eeprom_buffer_fill() before and one
// eeprom_buffer_flush() after (see the cost-model comment in the header).

void writeU16(uint16_t offset, uint16_t value) {
  eeprom_buffered_write_byte(offset, static_cast<uint8_t>(value & 0xFF));
  eeprom_buffered_write_byte(offset + 1, static_cast<uint8_t>((value >> 8) & 0xFF));
}

uint16_t readU16(uint16_t offset) {
  const uint8_t lo = eeprom_buffered_read_byte(offset);
  const uint8_t hi = eeprom_buffered_read_byte(offset + 1);
  return static_cast<uint16_t>(lo) | (static_cast<uint16_t>(hi) << 8);
}

void writeU32(uint16_t offset, uint32_t value) {
  for (int i = 0; i < 4; ++i) {
    eeprom_buffered_write_byte(offset + i, static_cast<uint8_t>((value >> (8 * i)) & 0xFF));
  }
}

uint32_t readU32(uint16_t offset) {
  uint32_t value = 0;
  for (int i = 0; i < 4; ++i) {
    value |= static_cast<uint32_t>(eeprom_buffered_read_byte(offset + i)) << (8 * i);
  }
  return value;
}

void writeStringField(const FieldSlot& slot, const std::string& value) {
  const uint16_t len = static_cast<uint16_t>(value.size() > slot.capacity ? slot.capacity : value.size());
  writeU16(slot.offset, len);
  for (uint16_t i = 0; i < len; ++i) {
    eeprom_buffered_write_byte(slot.offset + 2 + i, static_cast<uint8_t>(value[i]));
  }
}

void readStringField(const FieldSlot& slot, std::string& out) {
  uint16_t len = readU16(slot.offset);
  if (len > slot.capacity) len = slot.capacity;  // defensive: corrupt/blank flash.
  out.assign(len, '\0');
  for (uint16_t i = 0; i < len; ++i) {
    out[i] = static_cast<char>(eeprom_buffered_read_byte(slot.offset + 2 + i));
  }
}

}  // namespace

bool Stm32ArduinoStorage::begin(const char* ns) {
  if (_opened) return true;
  if (!ns || ns[0] == '\0') return false;

  eeprom_buffer_fill();
  const uint32_t magic = readU32(kMagicOffset);
  if (magic == kMagic) {
    _opened = true;
    return true;  // Already initialized: no need to spend a flash erase cycle.
  }

  // Blank (erased, reads as 0xFF) or foreign flash page: lay down a fresh,
  // empty record in one erase+reprogram pass.
  writeU32(kMagicOffset, kMagic);
  eeprom_buffered_write_byte(kVersionOffset, kLayoutVersion);
  for (const FieldSlot& slot : kFields) writeU16(slot.offset, 0);
  writeU32(kProfileVersionOffset, 0);
  writeU16(kMqttPortOffset, 0);
  eeprom_buffer_flush();

  _opened = true;
  return true;
}

bool Stm32ArduinoStorage::rawReadField(Field field, std::string& out) const {
  if (!_opened) return false;
  eeprom_buffer_fill();
  readStringField(kFields[static_cast<uint8_t>(field)], out);
  return true;
}

bool Stm32ArduinoStorage::rawWriteField(Field field, const std::string& value) {
  if (!_opened) return false;
  eeprom_buffer_fill();
  writeStringField(kFields[static_cast<uint8_t>(field)], value);
  eeprom_buffer_flush();
  return true;
}

bool Stm32ArduinoStorage::rawClearField(Field field) { return rawWriteField(field, std::string()); }

bool Stm32ArduinoStorage::hasWifi() const {
  if (!_opened) return false;
  eeprom_buffer_fill();
  return readU16(kFields[static_cast<uint8_t>(Field::kWifiSsid)].offset) > 0;
}

bool Stm32ArduinoStorage::loadWifiSsid(std::string& outSsid) const { return rawReadField(Field::kWifiSsid, outSsid); }

bool Stm32ArduinoStorage::saveWifiSsid(const std::string& ssid) {
  if (ssid.empty()) return false;
  return rawWriteField(Field::kWifiSsid, ssid);
}

bool Stm32ArduinoStorage::clearWifi() { return rawClearField(Field::kWifiSsid); }

bool Stm32ArduinoStorage::loadRuntimeIdentity(IoTLabsRuntimeIdentity& outIdentity) const {
  if (!_opened) return false;
  eeprom_buffer_fill();
  readStringField(kFields[static_cast<uint8_t>(Field::kDeviceId)], outIdentity.deviceId);
  readStringField(kFields[static_cast<uint8_t>(Field::kDeviceTemplateId)], outIdentity.deviceTemplateId);
  outIdentity.deviceTemplateVersion = readU32(kProfileVersionOffset);
  readStringField(kFields[static_cast<uint8_t>(Field::kMqttCid)], outIdentity.mqttClientId);
  readStringField(kFields[static_cast<uint8_t>(Field::kMqttUser)], outIdentity.mqttUsername);
  readStringField(kFields[static_cast<uint8_t>(Field::kMqttPass)], outIdentity.mqttPassword);
  readStringField(kFields[static_cast<uint8_t>(Field::kMqttHost)], outIdentity.mqttBroker);
  outIdentity.mqttPort = readU16(kMqttPortOffset);
  readStringField(kFields[static_cast<uint8_t>(Field::kTopicPref)], outIdentity.topicPrefix);
  return true;
}

bool Stm32ArduinoStorage::saveRuntimeIdentity(const IoTLabsRuntimeIdentity& identity) {
  if (!_opened || !identity.valid()) return false;

  // One fill + one flush for the whole identity record (9 fields), instead
  // of 9 separate rawWriteField() calls — see the cost-model comment in the
  // header for why that matters on this flash-emulated backend.
  eeprom_buffer_fill();
  writeStringField(kFields[static_cast<uint8_t>(Field::kDeviceId)], identity.deviceId);
  writeStringField(kFields[static_cast<uint8_t>(Field::kDeviceTemplateId)], identity.deviceTemplateId);
  writeU32(kProfileVersionOffset, identity.deviceTemplateVersion);
  writeStringField(kFields[static_cast<uint8_t>(Field::kMqttCid)], identity.mqttClientId);
  writeStringField(kFields[static_cast<uint8_t>(Field::kMqttUser)], identity.mqttUsername);
  writeStringField(kFields[static_cast<uint8_t>(Field::kMqttPass)], identity.mqttPassword);
  writeStringField(kFields[static_cast<uint8_t>(Field::kMqttHost)], identity.mqttBroker);
  writeU16(kMqttPortOffset, identity.mqttPort);
  writeStringField(kFields[static_cast<uint8_t>(Field::kTopicPref)], identity.topicPrefix);
  eeprom_buffer_flush();
  return true;
}

bool Stm32ArduinoStorage::clearRuntimeIdentity() {
  if (!_opened) return false;

  eeprom_buffer_fill();
  writeStringField(kFields[static_cast<uint8_t>(Field::kDeviceId)], std::string());
  writeStringField(kFields[static_cast<uint8_t>(Field::kDeviceTemplateId)], std::string());
  writeU32(kProfileVersionOffset, 0);
  writeStringField(kFields[static_cast<uint8_t>(Field::kMqttCid)], std::string());
  writeStringField(kFields[static_cast<uint8_t>(Field::kMqttUser)], std::string());
  writeStringField(kFields[static_cast<uint8_t>(Field::kMqttPass)], std::string());
  writeStringField(kFields[static_cast<uint8_t>(Field::kMqttHost)], std::string());
  writeU16(kMqttPortOffset, 0);
  writeStringField(kFields[static_cast<uint8_t>(Field::kTopicPref)], std::string());
  eeprom_buffer_flush();
  return true;
}

bool Stm32ArduinoStorage::clearAll() {
  if (!_opened) return false;

  eeprom_buffer_fill();
  for (const FieldSlot& slot : kFields) writeU16(slot.offset, 0);
  writeU32(kProfileVersionOffset, 0);
  writeU16(kMqttPortOffset, 0);
  eeprom_buffer_flush();
  return true;
}

#endif  // ARDUINO_ARCH_STM32
