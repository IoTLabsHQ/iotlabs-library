#include "AutomationEngine.h"

#include <cstring>

void AutomationEngine::clear() {
  for (uint8_t i = 0; i < MAX_RULES; ++i) _rules[i] = Rule();
}

bool AutomationEngine::parseOperator(const char* raw, Operator& out) {
  if (!raw) return false;
  if (strcmp(raw, ">") == 0) { out = Operator::GREATER_THAN; return true; }
  if (strcmp(raw, "<") == 0) { out = Operator::LESS_THAN; return true; }
  if (strcmp(raw, "==") == 0) { out = Operator::EQUAL; return true; }
  if (strcmp(raw, "!=") == 0) { out = Operator::NOT_EQUAL; return true; }
  return false;
}

bool AutomationEngine::evaluateOperator(Operator op, double actual, double expected) {
  switch (op) {
    case Operator::GREATER_THAN: return actual > expected;
    case Operator::LESS_THAN: return actual < expected;
    case Operator::EQUAL: return actual == expected;
    case Operator::NOT_EQUAL: return actual != expected;
  }
  return false;
}

// Parses `json` (an array of {id, trigger:{sourceControlId,operator,
// compareValue}, action:{targetControlId,value}}) into `_rules`, replacing
// whatever was there before. Entries with an unsupported shape are skipped,
// not fatal, per the class doc comment's V1 scope note.
bool AutomationEngine::loadPersisted(const std::string& json) {
  if (json.empty()) return false;
  if (json.size() > MAX_PAYLOAD_BYTES) return false;  // reject before parsing — no unbounded heap growth
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;  // corrupt/unrecognized — start empty, not fatal

  clear();
  uint8_t count = 0;
  for (JsonVariantConst entry : doc.as<JsonArrayConst>()) {
    if (count >= MAX_RULES) break;
    const char* id = entry["id"] | "";
    const char* sourceControlId = entry["trigger"]["sourceControlId"] | "";
    const char* operatorRaw = entry["trigger"]["operator"] | "";
    const char* targetControlId = entry["action"]["targetControlId"] | "";
    if (id[0] == '\0' || sourceControlId[0] == '\0' || targetControlId[0] == '\0') continue;

    Operator op;
    if (!parseOperator(operatorRaw, op)) continue;
    if (!entry["trigger"]["compareValue"].is<double>()) continue;
    if (!entry["action"]["value"].is<bool>()) continue;

    Rule& rule = _rules[count++];
    rule.used = true;
    rule.id = id;
    rule.sourceControlId = sourceControlId;
    rule.op = op;
    rule.compareValue = entry["trigger"]["compareValue"].as<double>();
    rule.targetControlId = targetControlId;
    rule.actionValue = entry["action"]["value"].as<bool>();
    rule.lastConditionResult = -1;
  }
  return count > 0;
}

void AutomationEngine::begin(IStorage& storage) {
  _storage = &storage;
  clear();
  std::string persisted;
  if (storage.loadAutomationRules(persisted)) loadPersisted(persisted);
}

uint8_t AutomationEngine::syncFromJson(JsonVariantConst rulesArray) {
  std::string serialized;
  serializeJson(rulesArray, serialized);
  loadPersisted(serialized);

  uint8_t count = 0;
  for (uint8_t i = 0; i < MAX_RULES; ++i) {
    if (_rules[i].used) count++;
  }

  if (_storage) {
    if (count > 0) {
      _storage->saveAutomationRules(serialized);
    } else {
      _storage->clearAutomationRules();
    }
  }
  return count;
}

void AutomationEngine::evaluate(const StateRegistry& states, ControlRouter& controls) {
  for (uint8_t i = 0; i < MAX_RULES; ++i) {
    Rule& rule = _rules[i];
    if (!rule.used) continue;

    double actual = 0.0;
    if (!states.getNumber(rule.sourceControlId.c_str(), actual)) continue;

    const bool result = evaluateOperator(rule.op, actual, rule.compareValue);
    const bool transitionedToTrue = result && rule.lastConditionResult != 1;
    rule.lastConditionResult = result ? 1 : 0;
    if (!transitionedToTrue) continue;

    JsonDocument valueDoc;
    valueDoc.set(rule.actionValue);
    bool handlerFound = false;
    controls.dispatch(rule.targetControlId.c_str(), valueDoc.as<JsonVariantConst>(), handlerFound);
  }
}

uint8_t AutomationEngine::ruleCount() const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < MAX_RULES; ++i) {
    if (_rules[i].used) count++;
  }
  return count;
}
