#include "Esp32SoftApProvisioningTransport.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <WiFi.h>

Esp32SoftApProvisioningTransport::Esp32SoftApProvisioningTransport() : _server(80) {}

bool Esp32SoftApProvisioningTransport::startAccessPoint(const char* apName, const char* apPassword) {
  if (_active) return true;

  WiFi.mode(WIFI_AP_STA);
  const bool ok = apPassword ? WiFi.softAP(apName, apPassword) : WiFi.softAP(apName);
  if (!ok) return false;

  registerRoutesOnce();
  _server.begin();
  _active = true;
  return true;
}

void Esp32SoftApProvisioningTransport::stopAccessPoint() {
  if (!_active) return;
  _server.stop();
  WiFi.softAPdisconnect(true);
  _active = false;
  if (WiFi.status() == WL_CONNECTED) WiFi.mode(WIFI_STA);
}

bool Esp32SoftApProvisioningTransport::accessPointActive() const { return _active; }

void Esp32SoftApProvisioningTransport::onDeviceInfoRequest(ProvisioningRequestHandler handler, void* context) {
  _deviceInfoHandler = handler;
  _deviceInfoContext = context;
}

void Esp32SoftApProvisioningTransport::onWifiNetworksRequest(ProvisioningRequestHandler handler, void* context) {
  _wifiNetworksHandler = handler;
  _wifiNetworksContext = context;
}

void Esp32SoftApProvisioningTransport::onProvisionRequest(ProvisioningRequestHandler handler, void* context) {
  _provisionHandler = handler;
  _provisionContext = context;
}

void Esp32SoftApProvisioningTransport::poll() { _server.handleClient(); }

void Esp32SoftApProvisioningTransport::setProvisioningPageTitle(const char* title) { _pageTitle = title; }

void Esp32SoftApProvisioningTransport::setDefaultActivationToken(const char* token) {
  _defaultActivationToken = token;
}

void Esp32SoftApProvisioningTransport::setDefaultWifiCredentials(const char* ssid, const char* password) {
  _defaultWifiSsid = ssid;
  _defaultWifiPassword = password;
}

void Esp32SoftApProvisioningTransport::dispatch(ProvisioningRequestHandler handler, void* context, bool withBody,
                                                 const String* bodyOverride) {
  if (!handler) {
    _server.send(500, "application/json", "{\"error\":{\"code\":\"NO_HANDLER\",\"message\":\"Route not wired\"}}");
    return;
  }

  const String body = bodyOverride ? *bodyOverride : (withBody ? _server.arg("plain") : String());
  IoTLabsHttpExchange exchange;
  exchange.requestBody = body.c_str();

  handler(context, exchange);

  _server.send(exchange.responseStatus, exchange.responseContentType, exchange.responseBody);
}

void Esp32SoftApProvisioningTransport::handleNotFound() {
  _server.send(404, "application/json", "{\"error\":{\"code\":\"NOT_FOUND\",\"message\":\"Unknown provisioning endpoint\"}}");
}

// Built-in default so every example gets a usable setup page with zero
// per-example HTML — examples override it via IOTLABS_PROJECT_NAME (picked
// up by IoTLabs::begin()) or IoTLabsConfig::provisioningPageTitle directly.
constexpr char kDefaultProvisioningPageTitle[] = "IoTLabs Device Setup";

const char kProvisioningPageHtmlTemplate[] PROGMEM = R"HTML(<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>%TITLE%</title>
  <style>
    body { font-family: sans-serif; max-width: 360px; margin: 24px auto; padding: 0 16px; }
    label { display: block; margin-top: 12px; font-size: 14px; }
    input { width: 100%; padding: 8px; font-size: 16px; box-sizing: border-box; }
    button { margin-top: 20px; width: 100%; padding: 12px; font-size: 16px; }
    #result { margin-top: 16px; white-space: pre-wrap; font-family: monospace; font-size: 13px; }
  </style>
</head>
<body>
  <h2>%TITLE%</h2>
  <p>%INTRO%</p>
  %WIFI_FIELDS%
  %TOKEN_FIELD%
  <button onclick="submitForm()">Provision</button>
  <div id="result"></div>
  <script>
    async function submitForm() {
      const result = document.getElementById('result');
      result.textContent = 'Submitting...';
      try {
        const params = new URLSearchParams({
          ssid: document.getElementById('ssid')?.value ?? '',
          password: document.getElementById('password')?.value ?? '',
          token: document.getElementById('token')?.value ?? '',
        });
        const res = await fetch('/apply', {
          method: 'POST',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
          body: params.toString(),
        });
        const text = await res.text();
        result.textContent = 'HTTP ' + res.status + '\n' + text;
      } catch (e) {
        result.textContent = 'Request failed: ' + e;
      }
    }
  </script>
</body>
</html>
)HTML";

void Esp32SoftApProvisioningTransport::handleProvisioningPage() {
  String html = String(kProvisioningPageHtmlTemplate);
  html.replace("%TITLE%", _pageTitle ? _pageTitle : kDefaultProvisioningPageTitle);

  const bool hasDefaultToken = _defaultActivationToken && strlen(_defaultActivationToken) > 0;
  const bool hasDefaultWifi = _defaultWifiSsid && strlen(_defaultWifiSsid) > 0;

  String intro;
  if (hasDefaultWifi && hasDefaultToken) {
    intro = "WiFi and the activation token are already configured in this firmware.";
  } else if (hasDefaultWifi) {
    intro = "WiFi is already configured in this firmware. Enter the activation token shown in Console.";
  } else if (hasDefaultToken) {
    intro = "Enter your WiFi. The activation token is already configured in this firmware.";
  } else {
    intro = "Enter your WiFi and the activation token shown in Console.";
  }
  html.replace("%INTRO%", intro);

  html.replace("%WIFI_FIELDS%",
               hasDefaultWifi ? ""
                               : "<label>WiFi SSID</label>\n  <input id=\"ssid\" type=\"text\" autocapitalize=\"off\">\n  "
                                 "<label>WiFi Password</label>\n  <input id=\"password\" type=\"password\">");
  html.replace("%TOKEN_FIELD%", hasDefaultToken
                                     ? ""
                                     : "<label>Activation Token</label>\n  <input id=\"token\" type=\"text\" autocapitalize=\"off\">");

  _server.send(200, "text/html", html);
}

// Escapes '"' and '\' so a raw String can be embedded safely inside a
// hand-built JSON string literal below — deliberately NOT ArduinoJson here:
// IProvisioningTransport.h's own contract keeps JSON parsing/building in
// core (ProvisioningManager), out of platform adapters.
String Esp32SoftApProvisioningTransport::jsonEscape(const String& value) {
  String out;
  out.reserve(value.length());
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out;
}

// WebServer parses application/x-www-form-urlencoded POST bodies into
// _server.arg(name) natively, so this route needs no JSON parsing at all —
// it just re-shapes 3 plain fields into the /provision contract's JSON body
// and dispatches in-process (no loopback HTTP hop, unlike the old ad-hoc
// per-example provisioning_page.h pattern this built-in page replaces).
void Esp32SoftApProvisioningTransport::handleProvisioningPageApply() {
  String ssid = _server.arg("ssid");
  String password = _server.arg("password");
  if (ssid.isEmpty() && _defaultWifiSsid && strlen(_defaultWifiSsid) > 0) {
    ssid = _defaultWifiSsid;
    password = _defaultWifiPassword ? _defaultWifiPassword : "";
  }
  String token = _server.arg("token");
  if (token.isEmpty() && _defaultActivationToken && strlen(_defaultActivationToken) > 0) {
    token = _defaultActivationToken;
  }

  const String body = "{\"wifi\":{\"ssid\":\"" + jsonEscape(ssid) + "\",\"password\":\"" + jsonEscape(password) +
                       "\"},\"activation\":{\"token\":\"" + jsonEscape(token) + "\"}}";
  dispatch(_provisionHandler, _provisionContext, true, &body);
}

void Esp32SoftApProvisioningTransport::registerRoutesOnce() {
  if (_routesRegistered) return;
  _routesRegistered = true;

  _server.on("/", HTTP_GET, [this]() { handleProvisioningPage(); });
  _server.on("/apply", HTTP_POST, [this]() { handleProvisioningPageApply(); });
  _server.on("/device-info", HTTP_GET, [this]() { dispatch(_deviceInfoHandler, _deviceInfoContext, false); });
  _server.on("/wifi/networks", HTTP_GET, [this]() { dispatch(_wifiNetworksHandler, _wifiNetworksContext, false); });
  _server.on("/provision", HTTP_POST, [this]() { dispatch(_provisionHandler, _provisionContext, true); });
  _server.onNotFound([this]() { handleNotFound(); });
}

#endif  // ARDUINO_ARCH_ESP32
