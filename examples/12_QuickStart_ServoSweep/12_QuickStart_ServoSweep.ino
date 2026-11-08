/*
  IoTLabs QuickStart 12 — Servo Sweep (onCommandFinish demo)

  Board reference: ESP32-C3 SuperMini
  Servo signal: GPIO4 (SG90-class hobby servo)

  Cloud Control:
    control_1_on_off   -- "Sweep": write true to start a non-blocking sweep
                           to the opposite end of travel; the device reports
                           it back to false on its own once the servo has
                           PHYSICALLY finished moving.
    control_2_value     -- "Servo Angle": last angle (degrees) the device
                           actually reached, read-only.

  Additional Arduino Library Manager dependency (NOT declared in any repo
  manifest -- install it yourself before compiling this example):
    arduino-cli lib install "ESP32Servo"

  ---------------------------------------------------------------------------
  WHY THIS EXAMPLE EXISTS -- onCommandFinish() vs. the write Control handler
  ---------------------------------------------------------------------------
  RuntimeManager::handleCommand() always calls publishStatus() right after a
  write Control's handler returns successfully -- this is existing SDK
  behavior. That auto-publish snapshots whatever is already cached in
  StateRegistry at that exact moment. For a control whose action completes
  INSTANTLY (e.g. a relay on/off), that is fine: the handler reports the new
  state synchronously and the snapshot already reflects it.

  For a control that kicks off something that takes real time -- like this
  servo sweep -- reporting the *final* state synchronously inside the write
  handler would be a LIE: the servo hasn't actually gotten there yet. That is
  exactly the gap onCommandFinish() exists to make visible, NOT to paper
  over:
    - onSweepControl() (the write handler below) returns IMMEDIATELY. It
      only starts a millis()-based state machine; it never blocks with
      delay() while the servo physically moves.
    - iot.onCommandFinish(onCommandFinish) fires once, right after ANY
      write Control's handler succeeds (not specific to "sweep"), just
      before the SDK's automatic publishStatus(). This example's callback
      deliberately does NOT call report()/reportBool() -- doing so here
      would just reproduce the SDK's existing auto-publish behavior a
      second time and add nothing.
    - The REAL completion report -- Sweep back to false, Servo Angle set to
      the angle actually reached -- happens LATER, from loop()'s own
      updateServoSweep(), only once currentAngle == targetAngle for real.

  See examples/05_Advanced_SmartWatering for the millis()-based non-blocking
  timed-cycle pattern this sketch follows (no delay() anywhere in loop()).
*/

#define IOTLABS_DEVICE_TEMPLATE_ID "PROF_REPLACE_ME"
#define IOTLABS_DEVICE_TEMPLATE_VERSION 1
#define IOTLABS_PROVISIONING_URL "https://vn.iotlabs.vn"
#define IOTLABS_FIRMWARE_VERSION "1.0.0"
#define IOTLABS_DEVICE_ID "DEV_your_device_id_here"  // REQUIRED: replace with your own Device's id from Console (create a Device there first — id looks like DEV_xxxxxxxx)
// #define IOTLABS_WIFI_SSID "your-wifi-ssid"        // optional: combined with the two macros above, skips the SoftAP portal entirely
// #define IOTLABS_WIFI_PASSWORD "your-wifi-password" // leave undefined/empty for an open network

#include <IoTLabs.h>
#include <ESP32Servo.h>

constexpr uint8_t SERVO_PIN = 4;
constexpr int ANGLE_MIN = 0;
constexpr int ANGLE_MAX = 180;
// Non-blocking step: advance 1 degree every STEP_INTERVAL_MS instead of
// calling delay() -- keeps iot.loop() (Wi-Fi/MQTT servicing) running the
// entire time the servo is moving.
constexpr uint32_t STEP_INTERVAL_MS = 15;

constexpr char CONTROL_SWEEP[] = "control_1_on_off";
constexpr char CONTROL_ANGLE[] = "control_2_value";

IoTLabs iot;
Servo servo;

int currentAngle = ANGLE_MIN;
int targetAngle = ANGLE_MIN;
bool sweeping = false;
uint32_t lastStepAt = 0;

// Starts the sweep and returns immediately -- no delay(), no blocking wait.
// Alternates target between the two ends of travel so repeated triggers
// sweep back and forth.
void startSweep() {
  if (sweeping) return;  // already mid-sweep: ignore re-trigger.

  targetAngle = (currentAngle <= (ANGLE_MIN + ANGLE_MAX) / 2) ? ANGLE_MAX : ANGLE_MIN;
  sweeping = true;
  lastStepAt = millis();

  Serial.print("[APP] Sweep started -> target=");
  Serial.println(targetAngle);

  // Safe to report "in progress" synchronously here: at this exact instant
  // it IS true, the handler just set it. This is not the async completion
  // report -- that only happens once the servo has actually arrived, see
  // updateServoSweep() below.
  iot.report(CONTROL_SWEEP, true);
}

// Called every loop() iteration. Advances the servo by one degree at most
// once per STEP_INTERVAL_MS -- this is the entire "non-blocking sweep"
// state machine. Only HERE, once currentAngle physically equals
// targetAngle, do we report the sweep as finished.
void updateServoSweep() {
  if (!sweeping) return;

  const uint32_t now = millis();
  if (now - lastStepAt < STEP_INTERVAL_MS) return;
  lastStepAt = now;

  if (currentAngle < targetAngle) {
    currentAngle++;
  } else if (currentAngle > targetAngle) {
    currentAngle--;
  }
  servo.write(currentAngle);

  if (currentAngle == targetAngle) {
    sweeping = false;

    // THE async completion report. Not from the write handler, not from
    // onCommandFinish() -- from loop()'s own detection that the sweep has
    // truly, physically finished.
    iot.report(CONTROL_SWEEP, false);
    iot.report(CONTROL_ANGLE, currentAngle);

    Serial.print("[APP] Sweep finished at angle=");
    Serial.println(currentAngle);
  }
}

// Write-Control handler for "Sweep". Must return immediately: it only
// starts the state machine above, it never waits for the servo to arrive.
void onSweepControl(bool value) {
  if (value) startSweep();
  // A write of `false` while idle is a no-op (nothing to stop); while a
  // real sweep is in progress it's also ignored here for simplicity --
  // "sweep" is a fire-and-forget trigger, not a hard on/off switch.
}

// Fires once per successfully-dispatched write Control, right before the
// SDK's automatic publishStatus() -- see the file-header comment above for
// why this callback deliberately does NOT call report()/reportBool().
void onCommandFinish(const char* controlId) {
  Serial.print("[APP] onCommandFinish: command dispatched for control=");
  Serial.println(controlId);
}

void onIoTLabsEvent(IoTLabsEvent event) {
  if (event == IoTLabsEvent::PROVISIONING_STARTED) {
    Serial.print("[IoTLabs] Provision using AP: ");
    Serial.println(iot.provisioningApName());
  } else if (event == IoTLabsEvent::ONLINE) {
    Serial.println("[IoTLabs] ONLINE");
  } else if (event == IoTLabsEvent::ERROR) {
    Serial.print("[IoTLabs] Error: ");
    Serial.println(iot.lastError());
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2400);
  servo.write(currentAngle);

  // Cache initial reported state without publishing yet.
  iot.report(CONTROL_SWEEP, false, false);
  iot.report(CONTROL_ANGLE, currentAngle, false);

  iot.on(CONTROL_SWEEP, onSweepControl);
  iot.onEvent(onIoTLabsEvent);
  iot.onCommandFinish(onCommandFinish);

  iot.begin();
}

void loop() {
  iot.loop();
  updateServoSweep();
}
