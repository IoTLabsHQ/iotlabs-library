#pragma once

// Ad-hoc addition, NOT part of IoTLabs Library: the library's own SoftAP
// server (port 80, Esp32SoftApProvisioningTransport) is a bare JSON REST
// API and its WebServer is a private member with no route-registration API
// exposed to application code (src/platform/esp32_arduino/
// Esp32SoftApProvisioningTransport.h/.cpp) — so a route can't be added to
// it directly. This runs a SECOND WebServer on port 8080 that serves a
// plain HTML form and, on submit, proxies the request device-side (via
// HTTPClient, over loopback) to the real /provision endpoint on port 80.
// From the phone's point of view the form and its submit target (/apply)
// are both on :8080 — same-origin, no CORS issue; the actual port-80 hop
// happens on the device, not in the phone's browser.
//
// This is per-example application code, not a library feature — see
// README.md's before/after comparison table for what this trades off.

#include <WebServer.h>
#include <HTTPClient.h>

WebServer provisioningWebServer(8080);

const char PROVISIONING_FORM_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>IoTLabs Smart Watering Setup</title>
  <style>
    body { font-family: sans-serif; max-width: 360px; margin: 24px auto; padding: 0 16px; }
    label { display: block; margin-top: 12px; font-size: 14px; }
    input { width: 100%; padding: 8px; font-size: 16px; box-sizing: border-box; }
    button { margin-top: 20px; width: 100%; padding: 12px; font-size: 16px; }
    #result { margin-top: 16px; white-space: pre-wrap; font-family: monospace; font-size: 13px; }
  </style>
</head>
<body>
  <h2>IoTLabs Smart Watering Setup</h2>
  <p>Enter your WiFi and the activation token shown in Console.</p>
  <label>WiFi SSID</label>
  <input id="ssid" type="text" autocapitalize="off">
  <label>WiFi Password</label>
  <input id="password" type="password">
  <label>Activation Token</label>
  <input id="token" type="text" autocapitalize="off">
  <button onclick="submitForm()">Provision</button>
  <div id="result"></div>
  <script>
    async function submitForm() {
      const result = document.getElementById('result');
      result.textContent = 'Submitting...';
      try {
        const res = await fetch('/apply', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            ssid: document.getElementById('ssid').value,
            password: document.getElementById('password').value,
            token: document.getElementById('token').value,
          }),
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

void handleProvisioningForm() {
  provisioningWebServer.send(200, "text/html", PROVISIONING_FORM_HTML);
}

// Proxies the form's simplified {ssid,password,token} body to the real
// IoTLabs Library /provision contract:
//   {"wifi":{"ssid":"...","password":"..."},"activation":{"token":"..."}}
// over loopback (127.0.0.1) — not through the WiFi/AP network stack.
void handleProvisioningApply() {
  JsonDocument formBody;
  const DeserializationError err = deserializeJson(formBody, provisioningWebServer.arg("plain"));
  if (err) {
    provisioningWebServer.send(400, "application/json", "{\"error\":\"invalid json\"}");
    return;
  }

  JsonDocument provisionBody;
  provisionBody["wifi"]["ssid"] = formBody["ssid"];
  provisionBody["wifi"]["password"] = formBody["password"];
  provisionBody["activation"]["token"] = formBody["token"];
  String payload;
  serializeJson(provisionBody, payload);

  HTTPClient http;
  http.begin("http://127.0.0.1/provision");
  http.addHeader("Content-Type", "application/json");
  const int status = http.POST(payload);
  const String responseBody = status > 0 ? http.getString() : http.errorToString(status);
  http.end();

  provisioningWebServer.send(status > 0 ? status : 502, "application/json", responseBody);
}

void beginProvisioningPage() {
  provisioningWebServer.on("/", HTTP_GET, handleProvisioningForm);
  provisioningWebServer.on("/apply", HTTP_POST, handleProvisioningApply);
  provisioningWebServer.begin();
}

void pollProvisioningPage() {
  provisioningWebServer.handleClient();
}
