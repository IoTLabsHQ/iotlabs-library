#pragma once

#include <cstdint>

// Public, platform-neutral types. No Arduino/platform headers on purpose:
// application code and every platform adapter both need to see this file.

#define IOTLABS_SDK_VERSION "1.0.0-rc.3"
#define IOTLABS_PROTOCOL_VERSION 1
#define IOTLABS_PROVISIONING_VERSION 1

struct IoTLabsConfig {
  const char* deviceTemplateId = nullptr;
  uint32_t deviceTemplateVersion = 1;
  const char* firmwareVersion = nullptr;

  // Device Platform bootstrap endpoint, e.g. https://vn.iotlabs.vn -- REQUIRED,
  // no hardcoded default, so a build can target its own regional deployment
  // (sg./vn./us.iotlabs.vn, or a self-hosted instance).
  const char* provisioningUrl = nullptr;
  const char* provisioningPath = "/provisioning";

  // Production default is secure TLS. Reference labs may explicitly enable insecure TLS.
  bool tlsInsecure = false;
  const char* provisioningRootCA = nullptr;
  const char* mqttRootCA = nullptr;

  const char* storageNamespace = "iotlabs";
  const char* provisioningApPrefix = "IoTLabs";
  const char* provisioningApPassword = nullptr; // null/empty => open AP; development only.

  // Title shown on the transport's built-in setup page, if it has one (see
  // IProvisioningTransport::setProvisioningPageTitle). null => transport's
  // own default. Ignored by transports with no such page (e.g. Linux).
  const char* provisioningPageTitle = nullptr;

  // Device Platform Device id (format "DEV_xxxxxxxx", copied from Console
  // when the Device Instance is created there, before the physical unit is
  // ever provisioned). REQUIRED -- validateConfig() rejects begin() with
  // DEVICE_ID_REQUIRED when this is null/empty. Drives the provisioning
  // SoftAP's SSID suffix (see ProvisioningManager::configure()) so Console
  // can predict/display the exact AP name a user needs to join, which is
  // not possible from the device's own hardwareId (unknown to the server
  // until the FIRST successful activation).
  const char* deviceId = nullptr;

  // Optional: when set, the SoftAP setup page skips asking for the
  // activation token and auto-fills submissions with this value instead
  // (see IProvisioningTransport::setDefaultActivationToken). null/absent
  // keeps today's behavior -- the page still shows the token field.
  const char* activationSecret = nullptr;

  // Optional, same idea as activationSecret but for WiFi: when both are
  // set, the SoftAP setup page hides the SSID/password fields too. If
  // wifiSsid AND activationSecret are BOTH set on a fresh/unprovisioned
  // device, ProvisioningManager::tryAutoProvision() skips the manual
  // portal step entirely -- WiFi connect + activation happen automatically
  // at boot, no captive-portal interaction needed at all. wifiPassword
  // null/empty means an open network (same convention as
  // provisioningApPassword above), not "not configured" -- wifiSsid alone
  // gates whether a default is considered present.
  const char* wifiSsid = nullptr;
  const char* wifiPassword = nullptr;

  uint32_t reconnectMinMs = 1000;
  uint32_t reconnectMaxMs = 30000;
  uint32_t wifiConnectTimeoutMs = 15000;
  uint32_t activationHttpTimeoutMs = 8000;
  uint32_t activationRetryMinMs = 2000;
  uint32_t activationRetryMaxMs = 30000;

  uint32_t telemetryIntervalMs = 10000;
  uint32_t statusIntervalMs = 30000;

  uint16_t keepAliveSeconds = 60;
  uint16_t socketTimeoutSeconds = 10;
  uint16_t mqttBufferBytes = 1536;
};

// CONTRACT (IoTLabs internal design notes): observable
// status values require compatibility review before changing.
enum class IoTLabsStatus : uint8_t {
  BOOT,
  UNPROVISIONED,
  PROVISIONING,
  WIFI_CONNECTING,
  ACTIVATING,
  CLOUD_CONNECTING,
  ONLINE,
  OFFLINE,
  ERROR
};

enum class IoTLabsEvent : uint8_t {
  BOOT,
  PROVISIONING_STARTED,
  PROVISIONING_ACCEPTED,
  WIFI_CONNECTING,
  WIFI_CONNECTED,
  WIFI_FAILED,
  ACTIVATING,
  ACTIVATED,
  CLOUD_CONNECTING,
  ONLINE,
  OFFLINE,
  PUBLISHED,
  MESSAGE_ERROR,
  ERROR,

  // Advisory only (setPinMapping() mapped a pin from the classic-ESP32
  // boot-strapping set -- 0/2/12/15 -- which can misbehave if pulled by
  // external circuitry during reset/flash). Does not flip IoTLabsStatus to
  // ERROR; additive to this enum, so existing switch/if-chains on prior
  // values are unaffected.
  PIN_MAPPING_STRAPPING_WARNING
};
