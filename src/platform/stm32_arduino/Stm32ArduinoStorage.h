#pragma once

#if defined(ARDUINO_ARCH_STM32)

#include <cstdint>

#include "../../core/ports/IStorage.h"

// STM32duino core 3.0.0's <EEPROM.h> backs EEPROM on STM32F4 (no genuine
// DATA_EEPROM peripheral on this family) with flash-emulated storage: the
// last flash page (FLASH_PAGE_SIZE, 8192 bytes by default and unredefined
// for the F427/F429/F437/F439 Nucleo-144 variant this adapter targets) is
// used as a single read/write byte-addressable region (E2END == 8191, so
// 8192 usable bytes total).
//
// IMPORTANT cost model: the *unbuffered* EEPROMClass API
// (EEPROM.write/EEPROM.put/EERef::operator=) calls eeprom_write_byte() per
// byte, and on this family (no DATA_EEPROM_BASE) every single call to that
// function does a full read-modify-erase-reprogram of the *entire* 8KB
// flash page. Writing this adapter's ~831-byte record one byte/field at a
// time through that API would trigger dozens of full-sector erase cycles
// per saveRuntimeIdentity() call — slow and needlessly wears the flash.
// Instead this adapter calls the lower-level buffered C API directly
// (eeprom_buffer_fill/eeprom_buffered_read_byte/eeprom_buffered_write_byte/
// eeprom_buffer_flush, declared in <EEPROM.h>'s utility/stm32_eeprom.h and
// linked with C linkage): eeprom_buffer_fill() loads the whole page into a
// RAM buffer (cheap, plain flash read), eeprom_buffered_write_byte() only
// mutates that RAM buffer, and eeprom_buffer_flush() does exactly ONE
// erase+reprogram of the whole page regardless of how many bytes changed.
// Every public method below therefore does at most one fill and, for
// writes, exactly one flush — never one flush per field.
//
// Fixed byte layout (all multi-byte integers little-endian; string fields
// are [uint16_t declaredLength][fixed capacity bytes], only the first
// `declaredLength` bytes of the capacity are meaningful on read):
//
//   offset  size  field
//   ------  ----  -----------------------------------------------------
//   0       4     magic (0x53544F49) — marks the page as an initialized
//                 IoTLabs record; a blank/erased flash page (0xFF bytes)
//                 or a foreign record is detected by this not matching.
//   4       1     layout version (currently 1)
//   5       34    wifi_ssid    (2-byte len + 32-byte capacity)
//   39      66    wifi_pass    (2-byte len + 64-byte capacity)
//   105     130   act_token    (2-byte len + 128-byte capacity)
//   235     66    device_id    (2-byte len + 64-byte capacity)
//   301     66    profile_id   (2-byte len + 64-byte capacity)
//   367     4     profile_ver  (uint32_t)
//   371     66    mqtt_cid     (2-byte len + 64-byte capacity)
//   437     66    mqtt_user    (2-byte len + 64-byte capacity)
//   503     130   mqtt_pass    (2-byte len + 128-byte capacity)
//   633     130   mqtt_host    (2-byte len + 128-byte capacity)
//   763     2     mqtt_port    (uint16_t)
//   765     66    topic_pref   (2-byte len + 64-byte capacity)
//   ------  ----
//   831 bytes used out of 8192 available (~10%); 7361 bytes reserved/unused.
//
// A value longer than its field's capacity is truncated on write (silently
// at the byte level, but every save*/ISecretStore save* caller that could
// plausibly overflow — act_token, mqtt_pass, mqtt_host — was given a
// generous 128-byte capacity specifically to make real-world truncation
// unlikely; this is a real, documented limitation of a fixed-record
// embedded store, not a claim that arbitrary-length strings are supported).
//
// `begin(const char* ns)` accepts (but ignores, beyond a non-null check
// mirroring the ESP32 adapter's contract) the namespace string: unlike
// ESP32's NVS/Preferences, this flash-page-backed emulation has no
// namespace/partition concept to select — there is exactly one fixed
// record, matching "storage adapter selected by board/core capabilities"
// from IoTLabs internal design notes
class Stm32ArduinoStorage : public IStorage {
 public:
  bool begin(const char* ns) override;

  bool hasWifi() const override;
  bool loadWifiSsid(std::string& outSsid) const override;
  bool saveWifiSsid(const std::string& ssid) override;
  bool clearWifi() override;

  bool loadRuntimeIdentity(IoTLabsRuntimeIdentity& outIdentity) const override;
  bool saveRuntimeIdentity(const IoTLabsRuntimeIdentity& identity) override;
  bool clearRuntimeIdentity() override;

  bool clearAll() override;

  // Exposed for Stm32ArduinoSecretStore composition only (mirrors
  // Esp32ArduinoStorage's rawGetString/rawPutString/rawRemove, but keyed by
  // a fixed slot index instead of a string key since this backend is a
  // fixed byte-offset record, not a key/value store).
  enum class Field : uint8_t {
    kWifiSsid = 0,
    kWifiPass,
    kActToken,
    kDeviceId,
    kDeviceTemplateId,
    kMqttCid,
    kMqttUser,
    kMqttPass,
    kMqttHost,
    kTopicPref,
  };

  bool rawReadField(Field field, std::string& out) const;
  bool rawWriteField(Field field, const std::string& value);
  bool rawClearField(Field field);

 private:
  bool _opened = false;
};

#endif  // ARDUINO_ARCH_STM32
