#pragma once

#include <Arduino.h>

#define IOTLABS_SDK_VERSION "1.0.0-rc.1"
#define IOTLABS_PROTOCOL_VERSION 1
#define IOTLABS_PROVISIONING_VERSION 1

struct IoTLabsConfig {
  const char* profileId = nullptr;
  uint32_t profileVersion = 1;
  const char* firmwareVersion = nullptr;

  // Device Platform bootstrap endpoint, e.g. https://device-api.example.com
  const char* devicePlatformUrl = nullptr;
  const char* activationPath = "/api/device/v1/activate";

  // Production default is secure TLS. Reference labs may explicitly enable insecure TLS.
  bool tlsInsecure = false;
  const char* devicePlatformRootCA = nullptr;
  const char* mqttRootCA = nullptr;

  const char* storageNamespace = "iotlabs";
  const char* provisioningApPrefix = "IoTLabs";
  const char* provisioningApPassword = nullptr; // null/empty => open AP; development only.

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
  ERROR
};

struct IoTLabsRuntimeIdentity {
  String deviceId;
  String profileId;
  uint32_t profileVersion = 1;

  String mqttClientId;
  String mqttUsername;
  String mqttPassword;
  String mqttBroker;
  uint16_t mqttPort = 8883;
  String topicPrefix;

  bool valid() const {
    return deviceId.length() > 0 && mqttClientId.length() > 0 &&
           mqttUsername.length() > 0 && mqttPassword.length() > 0 &&
           mqttBroker.length() > 0;
  }
};
