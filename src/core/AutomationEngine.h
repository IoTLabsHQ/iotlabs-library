#pragma once

#include <ArduinoJson.h>
#include <cstdint>
#include <string>

#include "ControlRouter.h"
#include "StateRegistry.h"
#include "ports/IStorage.h"

// Local ("edge") automation: rules synced down from Device Platform (via the
// existing /cmd channel, see RuntimeManager::handleCommand()'s
// "__automation_rules__" special-case), persisted to NVS, and evaluated
// every loop() tick against StateRegistry's cached reported values — no
// cloud round-trip needed to fire an action, so automation keeps working
// while the device is offline. Mirrors the shape of Device Platform's
// server-side AutomationRule (src/automations/schemas/automation-rule.schema.ts)
// so Console's existing rule-authoring UI needs no changes: same
// {trigger:{sourceControlId,operator,compareValue}, action:{targetControlId,value}}
// per rule, same edge-trigger guard (only fires on a false->true transition).
//
// V1 scope, deliberately narrow (see
// IoTLabs internal design notes
// §5.3): trigger compares a NUMBER control (int or float — StateRegistry::getNumber()
// covers both) against a numeric threshold; action sets a BOOL control. This
// covers the driving use case (sensor threshold -> actuator on/off, e.g.
// "power_usage > 2000 -> power off") without needing a fully generic
// value-type system. A rule whose action needs a non-bool value, or whose
// trigger needs a non-numeric comparison, is rejected at sync time (see
// syncFromJson()) rather than silently doing the wrong thing.
class AutomationEngine {
 public:
  static constexpr uint8_t MAX_RULES = 8;

  // Upper bound on the serialized JSON payload accepted by loadPersisted()
  // (both from a fresh cloud sync and from NVS-persisted storage). Rejected
  // outright, before deserializeJson() ever runs, so an oversized/malicious
  // sync payload can't force unbounded heap allocation on the device. Sized
  // generously for MAX_RULES=8 rules (~150-250 bytes each in practice) with
  // headroom for longer control ids.
  static constexpr size_t MAX_PAYLOAD_BYTES = 4096;

  // Loads any previously-persisted rule set (no-op, returns with zero rules,
  // on platforms whose IStorage doesn't implement automation persistence —
  // see IStorage.h).
  void begin(IStorage& storage);

  // Replaces the entire rule set from a synced JSON array (see file header
  // for shape) and persists it via the IStorage passed to begin(). Rules
  // with an unsupported trigger/action shape are skipped (not fatal — the
  // rest of the array still applies). Returns the number of rules accepted.
  uint8_t syncFromJson(JsonVariantConst rulesArray);

  // Call once per loop() tick. Reads each rule's source control's current
  // value from `states`, and on a false->true transition fires the action by
  // dispatching straight through `controls` — exactly as if the action's
  // value had arrived as a normal cloud command, so the app's own on()
  // handler runs unchanged (and its own report() call, if any, is what
  // syncs the new state back to cloud when connected).
  void evaluate(const StateRegistry& states, ControlRouter& controls);

  uint8_t ruleCount() const;

 private:
  enum class Operator : uint8_t { GREATER_THAN, LESS_THAN, EQUAL, NOT_EQUAL };

  struct Rule {
    bool used = false;
    std::string id;
    std::string sourceControlId;
    Operator op = Operator::GREATER_THAN;
    double compareValue = 0.0;
    std::string targetControlId;
    bool actionValue = false;
    // Tri-state edge-trigger guard, mirrors AutomationRule.lastConditionResult
    // on the server: -1 unknown (never evaluated), 0 false, 1 true.
    int8_t lastConditionResult = -1;
  };

  Rule _rules[MAX_RULES];
  IStorage* _storage = nullptr;

  void clear();
  bool loadPersisted(const std::string& json);
  static bool parseOperator(const char* raw, Operator& out);
  static bool evaluateOperator(Operator op, double actual, double expected);
};
