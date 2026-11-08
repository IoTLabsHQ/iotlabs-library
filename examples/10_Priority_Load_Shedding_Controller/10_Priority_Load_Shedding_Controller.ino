/*
  IoTLabs Advanced 10 — Priority Load Shedding Controller

  Source concept:
  https://maker.iotlabs.vn/projects/bo-dieu-khien-tu-dong-uu-tien-tai-khi-mat-dien-luoi

  Automatic priority load-shedding controller: detects grid (220V AC) loss
  via a PC817 optocoupler, sequentially disconnects non-essential loads to
  protect a backup power source (UPS/generator), monitors backup current via
  an ACS712, and restores loads (staged) when the grid comes back.

  ============================================================================
  SAFETY SCOPE — MAINS AC SENSING, READ BEFORE WIRING ANYTHING
  ============================================================================
  This project senses 220V AC grid presence and switches relays that carry
  AC mains loads. This firmware only implements the LOW-VOLTAGE control-
  signal side (ESP32 GPIO <-> PC817 opto input, ESP32 GPIO <-> relay module
  control inputs). It does NOT implement, and is explicitly out of scope
  for, the AC-side wiring itself:
    - Always install an RCD/breaker upstream before working on any 220V AC
      circuit.
    - NEVER connect an ESP32 GPIO directly to an AC circuit. The PC817
      optocoupler is the ONLY safe isolation between mains AC and this
      board — do NOT bypass it, do NOT substitute a direct resistor-divider
      sense line to AC.
    - ALL AC-side relay wiring (relay contacts switching real 220V AC
      loads) MUST be installed by a qualified electrician.
    - This firmware assumes that isolation and relay AC-side wiring are
      already correctly and safely installed; it only reads a GPIO fed by
      the PC817 output and drives relay-module control (low-voltage) pins.
  See IoTLabs internal design notes
  and 01_SPEC_KIEN_TRUC_HE_THONG.md for the full scope
  statement.
  ============================================================================

  Board: ESP32 DevKit V4 (classic ESP32, NOT the ESP32-C3 Supermini used by
  examples 07/08). FQBN: esp32:esp32:esp32. No CDCOnBoot quirk applies.

  GPIO:
    PC817 collector (grid-lost sense)  GPIO35  INPUT (input-only pin, see
                                                gotcha #1 below)
    PC817 emitter                      GND
    Relay IN1 — Essential              GPIO25  OUTPUT
    Relay IN2 — Important              GPIO26  OUTPUT
    Relay IN3 — Optional               GPIO27  OUTPUT
    Relay IN4 — Non-essential          GPIO14  OUTPUT
    ACS712 output (via divider)        GPIO34  INPUT (ADC1, input-only, see
                                                gotcha #2 below)
    Buzzer signal                      GPIO32  OUTPUT

  Hardware gotcha #1 — GPIO34/GPIO35 have NO internal pull-up/pull-down:
    These are ESP32 input-only pins. `pinMode(GRID_PIN, INPUT)` is the only
    valid mode — `INPUT_PULLUP` has no effect on these pins (silently
    ignored by the SDK, not an error). The pull-up to 3V3 on the PC817
    collector MUST be an external resistor on the board, or GRID_PIN will
    float and this firmware's readings will be meaningless.

  Hardware gotcha #2 — ACS712 5V bias exceeds the ESP32 3.3V ADC max input:
    ACS712 runs off 5V and its analog output is biased around mid-supply
    (~2.5V at zero current). That 2.5V bias, and any positive-current swing
    above it, EXCEEDS the ESP32 ADC's 3.3V max input. Wiring MUST include a
    voltage divider (or a 3.3V-native current sensor) between ACS712 Vout
    and GPIO34 — this is a REQUIRED wiring step, not just a firmware
    constant. Skipping it risks damaging the ESP32 ADC pin. See
    ACS712_DIVIDER_RATIO below — it is a PLACEHOLDER that MUST be
    calibrated against the actual resistor pair installed, same pattern as
    example 09_Solar_Energy_Monitoring_Control's battery divider.

  Relay module polarity assumption: this firmware is written for a typical
  5V relay module with ACTIVE-LOW control inputs (IN pin LOW = relay
  energized/ON, HIGH = de-energized/OFF) — the most common wiring for
  cheap 4-channel relay modules. If your module is active-HIGH, flip
  RELAY_ACTIVE_LEVEL below. See 01_SPEC_KIEN_TRUC_HE_THONG.md.

  Controls (see PROFILE_INTENT.json):
    control_1_on_off  grid_lost           read       — debounced grid state
    control_2_on_off  relay_essential     read_write — never auto-shed
    control_3_on_off  relay_important     read_write — shed 2nd (tier 2)
    control_4_on_off  relay_optional      read_write — shed 1st (tier 1)
    control_5_on_off  relay_nonessential  read_write — shed immediately on
                                           grid loss (mandatory cut)
    control_6_value   backup_current      read       (A)
    control_7_value   current_limit       read_write (A, 1-15, default 8.0)
    control_8_on_off  buzzer_alarm        read       — continuous while
                                           OVERCURRENT_CRITICAL
    control_9_text    system_status       read       — NORMAL /
                                           GRID_LOST / GRID_LOST_SHEDDING /
                                           OVERCURRENT_CRITICAL

  Priority-shedding state machine (full spec: 02_PRD_CONTROLS_VA_AUTOMATION.md):
    - Grid-loss debounce: raw GRID_PIN sampled every ~20ms; a state
      transition is only confirmed once >=90% of samples in a rolling
      ~400ms window agree with the new state. Mains-referenced opto
      circuits toggle at line frequency while grid is present and only go
      steady when grid is actually lost — a naive single-sample read would
      false-trigger. GRID_DEBOUNCE_WINDOW_MS is a tunable constant; the
      exact timing NEEDS VALIDATION ON REAL HARDWARE (see
      04_QA_TEST_PLAN.md) — not yet confirmed against a real PC817 circuit.
    - Confirmed grid_lost=true: immediately shed relay_nonessential.
      Essential/important/optional stay on (running off backup).
      system_status = GRID_LOST.
    - While grid_lost, every SETTLE_DELAY_MS (2s): if backup_current >
      current_limit, shed the next-lowest active tier (optional, then
      important — essential is NEVER auto-shed). system_status =
      GRID_LOST_SHEDDING while any tier beyond nonessential has been shed.
    - If backup_current still exceeds current_limit with only essential
      left on: buzzer_alarm = true continuously, system_status =
      OVERCURRENT_CRITICAL. Firmware does not cut essential automatically —
      this needs a human to intervene.
    - Confirmed grid restore (grid_lost debounced back to false): restore
      relays in reverse shed order (important -> optional -> nonessential)
      with a stagger delay (default 750ms) between each, to avoid combined
      inrush current. Clears buzzer_alarm, system_status = NORMAL.
    - Manual relay writes via iot.on() are honored immediately (for
      testing/override), but the state machine re-asserts (forces back off)
      any tier that should currently be shed on the very next loop()
      iteration while grid is still lost — the state machine wins over a
      stale manual toggle during an active shedding event.
    - Every control is reported on state change, not just on write.

  Provisioning page: built-in IoTLabs Library feature
  (Esp32SoftApProvisioningTransport), no custom provisioning_page.h needed.
*/

#define IOTLABS_PROJECT_NAME "Load Shedding Controller Setup"
#define IOTLABS_DEVICE_TEMPLATE_ID "PROF_REPLACE_ME"
#define IOTLABS_DEVICE_TEMPLATE_VERSION 1
#define IOTLABS_PROVISIONING_URL "https://vn.iotlabs.vn"
#define IOTLABS_FIRMWARE_VERSION "1.0.0"
#define IOTLABS_DEVICE_ID "DEV_your_device_id_here"  // REQUIRED: replace with your own Device's id from Console (create a Device there first — id looks like DEV_xxxxxxxx)
// #define IOTLABS_WIFI_SSID "your-wifi-ssid"        // optional: combined with the two macros above, skips the SoftAP portal entirely
// #define IOTLABS_WIFI_PASSWORD "your-wifi-password" // leave undefined/empty for an open network

#include <IoTLabs.h>
#include <string.h>

// ---- GPIO --------------------------------------------------------------
constexpr uint8_t GRID_PIN = 35;                // input-only, NO internal pull — external pull-up required
constexpr uint8_t RELAY_ESSENTIAL_PIN = 25;
constexpr uint8_t RELAY_IMPORTANT_PIN = 26;
constexpr uint8_t RELAY_OPTIONAL_PIN = 27;
constexpr uint8_t RELAY_NONESSENTIAL_PIN = 14;
constexpr uint8_t ACS712_PIN = 34;              // input-only ADC1 — REQUIRES external divider, see gotcha #2
constexpr uint8_t BUZZER_PIN = 32;

// ---- Control IDs — match PROFILE_INTENT.json ----------------------------
constexpr char CONTROL_GRID_LOST[] = "control_1_on_off";
constexpr char CONTROL_RELAY_ESSENTIAL[] = "control_2_on_off";
constexpr char CONTROL_RELAY_IMPORTANT[] = "control_3_on_off";
constexpr char CONTROL_RELAY_OPTIONAL[] = "control_4_on_off";
constexpr char CONTROL_RELAY_NONESSENTIAL[] = "control_5_on_off";
constexpr char CONTROL_BACKUP_CURRENT[] = "control_6_value";
constexpr char CONTROL_CURRENT_LIMIT[] = "control_7_value";
constexpr char CONTROL_BUZZER_ALARM[] = "control_8_on_off";
constexpr char CONTROL_SYSTEM_STATUS[] = "control_9_text";

// ---- Relay module polarity — TUNE to your actual relay module ----------
// Most cheap 5V 4-channel relay modules are ACTIVE-LOW (IN=LOW energizes
// the relay). Flip this to HIGH if your module is active-HIGH.
constexpr int RELAY_ACTIVE_LEVEL = LOW;

// ---- Grid-loss debounce — TUNE, needs real-hardware validation ---------
// See file header + 04_QA_TEST_PLAN.md: exact timing not yet confirmed
// against a real PC817 circuit on real mains.
constexpr uint32_t GRID_SAMPLE_INTERVAL_MS = 20;
constexpr uint32_t GRID_DEBOUNCE_WINDOW_MS = 400;
constexpr uint8_t GRID_WINDOW_SAMPLES = GRID_DEBOUNCE_WINDOW_MS / GRID_SAMPLE_INTERVAL_MS;  // 20
constexpr uint8_t GRID_STABLE_THRESHOLD_PCT = 90;

// ---- Shedding / restore timing ------------------------------------------
constexpr uint32_t SETTLE_DELAY_MS = 2000;    // between successive shed checks while over limit
constexpr uint32_t STAGGER_DELAY_MS = 750;    // between successive restore steps (500ms-1s range)
constexpr uint32_t CURRENT_READ_INTERVAL_MS = 500;

// ---- ACS712 read — oversampled ADC, see 05_Advanced_SmartWatering pattern
constexpr int ADC_SAMPLE_COUNT = 20;
constexpr uint16_t ADC_MAX_VALUE = 4095;   // 12-bit (analogReadResolution(12))
constexpr float ADC_REF_VOLTAGE_MV = 3300.0f;

// ACS712 electrical constants — TUNE to your actual sensor variant/wiring.
// ACS712_DIVIDER_RATIO is a PLACEHOLDER: (R_top + R_bottom) / R_bottom of
// the required divider between ACS712 Vout and GPIO34 (see gotcha #2) —
// MUST be calibrated against the actual resistor pair installed, same
// pattern as example 09's BATTERY_DIVIDER_RATIO.
constexpr float ACS712_DIVIDER_RATIO = 2.0f;      // CALIBRATE with your actual resistor values
constexpr float ACS712_QUIESCENT_MV = 2500.0f;    // ~VCC/2 at 0A for a 5V-supplied ACS712
constexpr float ACS712_MV_PER_AMP = 100.0f;       // ACS712-20A variant assumption (100mV/A) — TUNE for 5A/30A variants

// ---- Current limit — default per source project, adjustable at runtime -
constexpr float CURRENT_LIMIT_DEFAULT_A = 8.0f;
constexpr float CURRENT_LIMIT_MIN_A = 1.0f;
constexpr float CURRENT_LIMIT_MAX_A = 15.0f;

// ---- system_status string constants -------------------------------------
constexpr char STATUS_NORMAL[] = "NORMAL";
constexpr char STATUS_GRID_LOST[] = "GRID_LOST";
constexpr char STATUS_GRID_LOST_SHEDDING[] = "GRID_LOST_SHEDDING";
constexpr char STATUS_OVERCURRENT_CRITICAL[] = "OVERCURRENT_CRITICAL";

enum RelayTier : uint8_t {
  TIER_ESSENTIAL = 0,
  TIER_IMPORTANT = 1,
  TIER_OPTIONAL = 2,
  TIER_NONESSENTIAL = 3,
  TIER_COUNT = 4,
};

struct RelayInfo {
  uint8_t pin;
  const char* controlId;
  bool on;
};

enum RestoreStage : uint8_t {
  RESTORE_STAGE_IMPORTANT = 0,
  RESTORE_STAGE_OPTIONAL = 1,
  RESTORE_STAGE_NONESSENTIAL = 2,
  RESTORE_STAGE_DONE = 3,
};

IoTLabs iot;

RelayInfo relays[TIER_COUNT] = {
  {RELAY_ESSENTIAL_PIN, CONTROL_RELAY_ESSENTIAL, true},
  {RELAY_IMPORTANT_PIN, CONTROL_RELAY_IMPORTANT, true},
  {RELAY_OPTIONAL_PIN, CONTROL_RELAY_OPTIONAL, true},
  {RELAY_NONESSENTIAL_PIN, CONTROL_RELAY_NONESSENTIAL, true},
};

// Grid debounce state — rolling window of raw samples (HIGH = opto not
// conducting = grid-lost candidate, see 01_SPEC_KIEN_TRUC_HE_THONG.md).
bool gridSampleWindow[GRID_WINDOW_SAMPLES];
uint8_t gridSampleIndex = 0;
uint8_t gridSampleCount = 0;
uint8_t gridHighCount = 0;
uint32_t lastGridSampleAt = 0;
bool gridLost = false;  // confirmed/debounced state; boot-time default assumes grid present

// Shedding state machine
uint8_t shedFrontier = 0;   // 0 = only mandatory nonessential cut so far, 1 = +optional, 2 = +important
bool overcurrentCritical = false;
uint32_t nextShedCheckAt = 0;

// Staged restore state machine
uint8_t restoreStage = RESTORE_STAGE_DONE;
uint32_t nextRestoreActionAt = 0;

// Current sensing
float backupCurrentAmps = 0.0f;
float currentLimitAmps = CURRENT_LIMIT_DEFAULT_A;
uint32_t lastCurrentReadAt = 0;

// Buzzer / status
bool buzzerOn = false;
char lastSystemStatus[24] = "";

void applyRelay(RelayTier tier, bool on) {
  relays[tier].on = on;
  digitalWrite(relays[tier].pin, on ? RELAY_ACTIVE_LEVEL : !RELAY_ACTIVE_LEVEL);
}

void reportRelay(RelayTier tier, bool publishNow = true) {
  iot.report(relays[tier].controlId, relays[tier].on, publishNow);
}

void reportGridLostState(bool publishNow = true) {
  iot.report(CONTROL_GRID_LOST, gridLost, publishNow);
}

void setBuzzer(bool on) {
  if (buzzerOn == on) return;
  buzzerOn = on;
  digitalWrite(BUZZER_PIN, buzzerOn ? HIGH : LOW);
  iot.report(CONTROL_BUZZER_ALARM, buzzerOn);
}

// system_status is derived, not stored as an independent state — always a
// pure function of (gridLost, overcurrentCritical, shedFrontier).
const char* computeSystemStatus() {
  if (!gridLost) return STATUS_NORMAL;
  if (overcurrentCritical) return STATUS_OVERCURRENT_CRITICAL;
  if (shedFrontier > 0) return STATUS_GRID_LOST_SHEDDING;
  return STATUS_GRID_LOST;
}

void updateSystemStatus(bool publishNow = true) {
  const char* status = computeSystemStatus();
  if (strcmp(status, lastSystemStatus) == 0) return;
  strncpy(lastSystemStatus, status, sizeof(lastSystemStatus) - 1);
  lastSystemStatus[sizeof(lastSystemStatus) - 1] = '\0';
  iot.report(CONTROL_SYSTEM_STATUS, lastSystemStatus, publishNow);
}

// Confirmed grid loss: mandatory immediate cut of relay_nonessential.
// Essential/important/optional stay on (running off backup). Any pending
// staged restore is aborted — we are back into shed mode.
void onGridLost() {
  restoreStage = RESTORE_STAGE_DONE;
  shedFrontier = 0;
  overcurrentCritical = false;
  applyRelay(TIER_NONESSENTIAL, false);
  reportRelay(TIER_NONESSENTIAL);
  // Let the nonessential-cut transient settle before the first current
  // re-check (avoids reacting to a brief switching transient).
  nextShedCheckAt = millis() + SETTLE_DELAY_MS;
  updateSystemStatus();
}

// Confirmed grid restore: clear alarm immediately, then begin the staged
// (staggered) reconnection important -> optional -> nonessential, ticked
// in updateStagedRestore(). system_status flips to NORMAL right away per
// spec, independent of how long the staged reconnection takes to finish.
void onGridRestored() {
  setBuzzer(false);
  overcurrentCritical = false;
  shedFrontier = 0;
  restoreStage = RESTORE_STAGE_IMPORTANT;
  nextRestoreActionAt = millis();  // act on the first stage right away
  updateSystemStatus();
}

// Non-blocking debounce: samples GRID_PIN every ~20ms into a rolling
// ~400ms window; only confirms a state transition once >=90% of the
// window agrees with the new state. See file header for why a naive
// single-sample read would false-trigger on a mains-referenced opto.
void updateGridDebounce() {
  const uint32_t now = millis();
  if (now - lastGridSampleAt < GRID_SAMPLE_INTERVAL_MS) return;
  lastGridSampleAt = now;

  const bool raw = (digitalRead(GRID_PIN) == HIGH);  // HIGH = opto not conducting = grid-lost candidate

  if (gridSampleCount == GRID_WINDOW_SAMPLES) {
    if (gridSampleWindow[gridSampleIndex]) gridHighCount--;
  } else {
    gridSampleCount++;
  }
  gridSampleWindow[gridSampleIndex] = raw;
  if (raw) gridHighCount++;
  gridSampleIndex = (gridSampleIndex + 1) % GRID_WINDOW_SAMPLES;

  if (gridSampleCount < GRID_WINDOW_SAMPLES) return;  // wait for the window to fill before deciding

  const uint8_t highPct = static_cast<uint8_t>((static_cast<uint16_t>(gridHighCount) * 100) / GRID_WINDOW_SAMPLES);

  if (!gridLost && highPct >= GRID_STABLE_THRESHOLD_PCT) {
    gridLost = true;
    reportGridLostState();
    onGridLost();
  } else if (gridLost && highPct <= (100 - GRID_STABLE_THRESHOLD_PCT)) {
    gridLost = false;
    reportGridLostState();
    onGridRestored();
  }
}

int readAdcRawAverage(uint8_t pin) {
  long total = 0;
  for (int i = 0; i < ADC_SAMPLE_COUNT; i++) {
    total += analogRead(pin);
    delay(2);
  }
  return static_cast<int>(total / ADC_SAMPLE_COUNT);
}

// Reconstructs the real ACS712 output voltage from the divided ADC
// reading, then converts to amps. See gotcha #2 + ACS712_* constants above
// — divider ratio and mV/A MUST be calibrated/confirmed for the actual
// hardware installed.
float readBackupCurrentAmps() {
  const int raw = readAdcRawAverage(ACS712_PIN);
  const float adcVoltageMv = (static_cast<float>(raw) / static_cast<float>(ADC_MAX_VALUE)) * ADC_REF_VOLTAGE_MV;
  const float acsOutputMv = adcVoltageMv * ACS712_DIVIDER_RATIO;
  return (acsOutputMv - ACS712_QUIESCENT_MV) / ACS712_MV_PER_AMP;
}

void updateCurrentSensor() {
  const uint32_t now = millis();
  if (now - lastCurrentReadAt < CURRENT_READ_INTERVAL_MS) return;
  lastCurrentReadAt = now;
  backupCurrentAmps = readBackupCurrentAmps();
  iot.report(CONTROL_BACKUP_CURRENT, backupCurrentAmps);
  printStatus();
}

// Only meaningful while grid_lost. Every SETTLE_DELAY_MS, re-checks
// backup_current against current_limit and advances the shed frontier
// (optional -> important, essential NEVER auto-shed) or raises the
// overcurrent-critical alarm once only essential remains.
void updateSheddingDecision() {
  if (!gridLost) return;
  if (millis() < nextShedCheckAt) return;
  nextShedCheckAt = millis() + SETTLE_DELAY_MS;

  if (backupCurrentAmps > currentLimitAmps) {
    if (shedFrontier == 0) {
      applyRelay(TIER_OPTIONAL, false);
      reportRelay(TIER_OPTIONAL);
      shedFrontier = 1;
    } else if (shedFrontier == 1) {
      applyRelay(TIER_IMPORTANT, false);
      reportRelay(TIER_IMPORTANT);
      shedFrontier = 2;
    } else if (!overcurrentCritical) {
      overcurrentCritical = true;
      setBuzzer(true);
    }
  } else if (overcurrentCritical) {
    overcurrentCritical = false;
    setBuzzer(false);
  }

  updateSystemStatus();
}

// Re-asserts the currently-shed tiers every loop() iteration while grid is
// lost. A manual write via iot.on() below is honored immediately (for
// testing/override), but if the tier is within the active shed set, this
// forces it back off on the very next iteration — the state machine wins
// over a stale manual toggle during an active shedding event.
void enforceShedState() {
  if (!gridLost) return;
  if (relays[TIER_NONESSENTIAL].on) {
    applyRelay(TIER_NONESSENTIAL, false);
    reportRelay(TIER_NONESSENTIAL);
  }
  if (shedFrontier >= 1 && relays[TIER_OPTIONAL].on) {
    applyRelay(TIER_OPTIONAL, false);
    reportRelay(TIER_OPTIONAL);
  }
  if (shedFrontier >= 2 && relays[TIER_IMPORTANT].on) {
    applyRelay(TIER_IMPORTANT, false);
    reportRelay(TIER_IMPORTANT);
  }
}

// Ticks the staged reconnection important -> optional -> nonessential with
// STAGGER_DELAY_MS between steps, to avoid combined inrush current from
// reconnecting all shed loads at once.
void updateStagedRestore() {
  if (restoreStage == RESTORE_STAGE_DONE) return;
  if (millis() < nextRestoreActionAt) return;

  switch (restoreStage) {
    case RESTORE_STAGE_IMPORTANT:
      if (!relays[TIER_IMPORTANT].on) {
        applyRelay(TIER_IMPORTANT, true);
        reportRelay(TIER_IMPORTANT);
      }
      restoreStage = RESTORE_STAGE_OPTIONAL;
      nextRestoreActionAt = millis() + STAGGER_DELAY_MS;
      break;
    case RESTORE_STAGE_OPTIONAL:
      if (!relays[TIER_OPTIONAL].on) {
        applyRelay(TIER_OPTIONAL, true);
        reportRelay(TIER_OPTIONAL);
      }
      restoreStage = RESTORE_STAGE_NONESSENTIAL;
      nextRestoreActionAt = millis() + STAGGER_DELAY_MS;
      break;
    case RESTORE_STAGE_NONESSENTIAL:
      if (!relays[TIER_NONESSENTIAL].on) {
        applyRelay(TIER_NONESSENTIAL, true);
        reportRelay(TIER_NONESSENTIAL);
      }
      restoreStage = RESTORE_STAGE_DONE;
      break;
    default:
      break;
  }
}

// ---- Console/Mobile write handlers --------------------------------------
// relay_essential is never touched by the shedding/restore state machine —
// a manual write here is simply honored, no re-assertion ever applies.
void onRelayEssentialControl(bool on) {
  applyRelay(TIER_ESSENTIAL, on);
  reportRelay(TIER_ESSENTIAL);
}

// Manual override honored immediately; enforceShedState() forces this back
// off on the next loop() iteration if grid is still lost and this tier is
// within the active shed set (shedFrontier >= 2).
void onRelayImportantControl(bool on) {
  applyRelay(TIER_IMPORTANT, on);
  reportRelay(TIER_IMPORTANT);
}

// Same precedence rule as important, at shedFrontier >= 1.
void onRelayOptionalControl(bool on) {
  applyRelay(TIER_OPTIONAL, on);
  reportRelay(TIER_OPTIONAL);
}

// Nonessential is the mandatory cut for the entire grid_lost duration —
// enforceShedState() forces it back off on the next loop() iteration
// regardless of shedFrontier, whenever grid is lost.
void onRelayNonessentialControl(bool on) {
  applyRelay(TIER_NONESSENTIAL, on);
  reportRelay(TIER_NONESSENTIAL);
}

void onCurrentLimitControl(float value) {
  if (value < CURRENT_LIMIT_MIN_A || value > CURRENT_LIMIT_MAX_A) {
    iot.report(CONTROL_CURRENT_LIMIT, currentLimitAmps);  // reject out-of-range, echo current value
    return;
  }
  currentLimitAmps = value;
  iot.report(CONTROL_CURRENT_LIMIT, currentLimitAmps);
}

void onIoTLabsEvent(IoTLabsEvent event) {
  if (event == IoTLabsEvent::PROVISIONING_STARTED) {
    Serial.print("[APP] Provisioning — connect to: ");
    Serial.println(iot.provisioningApName());
  } else if (event == IoTLabsEvent::ONLINE) {
    Serial.println("[APP] ONLINE");
  } else if (event == IoTLabsEvent::ERROR) {
    Serial.print("[APP] Error: ");
    Serial.println(iot.lastError());
  }
}

void printStatus() {
  Serial.print("[APP] grid_lost=");
  Serial.print(gridLost ? "1" : "0");
  Serial.print(" status=");
  Serial.print(lastSystemStatus);
  Serial.print(" current=");
  Serial.print(backupCurrentAmps);
  Serial.print("A limit=");
  Serial.print(currentLimitAmps);
  Serial.print("A relays(E/I/O/N)=");
  Serial.print(relays[TIER_ESSENTIAL].on ? "1" : "0");
  Serial.print(relays[TIER_IMPORTANT].on ? "1" : "0");
  Serial.print(relays[TIER_OPTIONAL].on ? "1" : "0");
  Serial.print(relays[TIER_NONESSENTIAL].on ? "1" : "0");
  Serial.print(" buzzer=");
  Serial.println(buzzerOn ? "1" : "0");
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(GRID_PIN, INPUT);  // input-only pin, no internal pull — see gotcha #1
  pinMode(RELAY_ESSENTIAL_PIN, OUTPUT);
  pinMode(RELAY_IMPORTANT_PIN, OUTPUT);
  pinMode(RELAY_OPTIONAL_PIN, OUTPUT);
  pinMode(RELAY_NONESSENTIAL_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Safe boot default: assume grid present, all loads powered. Actual
  // state is (re-)confirmed by the debounce loop within ~GRID_DEBOUNCE_WINDOW_MS.
  applyRelay(TIER_ESSENTIAL, true);
  applyRelay(TIER_IMPORTANT, true);
  applyRelay(TIER_OPTIONAL, true);
  applyRelay(TIER_NONESSENTIAL, true);
  digitalWrite(BUZZER_PIN, LOW);

  analogReadResolution(12);

  // Seed the debounce window with the boot-time raw reading so a real
  // grid-lost condition present at boot is confirmed on the very first
  // full window rather than needing GRID_DEBOUNCE_WINDOW_MS of seeded
  // "grid present" samples to override first.
  const bool bootRaw = (digitalRead(GRID_PIN) == HIGH);
  for (uint8_t i = 0; i < GRID_WINDOW_SAMPLES; i++) gridSampleWindow[i] = bootRaw;
  gridSampleIndex = 0;
  gridSampleCount = GRID_WINDOW_SAMPLES;
  gridHighCount = bootRaw ? GRID_WINDOW_SAMPLES : 0;

  strncpy(lastSystemStatus, computeSystemStatus(), sizeof(lastSystemStatus) - 1);

  reportGridLostState(false);
  for (uint8_t t = 0; t < TIER_COUNT; t++) iot.report(relays[t].controlId, relays[t].on, false);
  iot.report(CONTROL_BACKUP_CURRENT, backupCurrentAmps, false);
  iot.report(CONTROL_CURRENT_LIMIT, currentLimitAmps, false);
  iot.report(CONTROL_BUZZER_ALARM, buzzerOn, false);
  iot.report(CONTROL_SYSTEM_STATUS, lastSystemStatus, false);

  iot.on(CONTROL_RELAY_ESSENTIAL, onRelayEssentialControl);
  iot.on(CONTROL_RELAY_IMPORTANT, onRelayImportantControl);
  iot.on(CONTROL_RELAY_OPTIONAL, onRelayOptionalControl);
  iot.on(CONTROL_RELAY_NONESSENTIAL, onRelayNonessentialControl);
  iot.on(CONTROL_CURRENT_LIMIT, onCurrentLimitControl);
  iot.onEvent(onIoTLabsEvent);

  iot.begin();
}

void loop() {
  iot.loop();
  updateGridDebounce();
  updateCurrentSensor();
  updateSheddingDecision();
  enforceShedState();
  updateStagedRestore();
}
