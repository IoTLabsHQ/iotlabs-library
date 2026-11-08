#include "PinMappingLogic.h"

void PinMappingLogic::seedInitialState(bool rawHigh, uint32_t nowMs) {
  const bool rawActive = _digitalInCfg.activeLow ? !rawHigh : rawHigh;
  _lastRaw = rawActive;
  _hasLastRaw = true;
  _lastChangeAt = nowMs;
  _stableActive = rawActive;
  _hasStable = true;
}

PinMappingLogic::Edge PinMappingLogic::feedDigitalSample(bool rawHigh, uint32_t nowMs, bool& outActiveState) {
  const bool rawActive = _digitalInCfg.activeLow ? !rawHigh : rawHigh;

  if (!_hasLastRaw || rawActive != _lastRaw) {
    _lastRaw = rawActive;
    _hasLastRaw = true;
    _lastChangeAt = nowMs;
  }

  Edge edge = Edge::NONE;
  if ((nowMs - _lastChangeAt) >= _digitalInCfg.debounceMs && (!_hasStable || _stableActive != _lastRaw)) {
    const bool wasActive = _hasStable && _stableActive;
    _stableActive = _lastRaw;
    _hasStable = true;
    if (_stableActive) {
      edge = Edge::BECAME_ACTIVE;
    } else if (wasActive) {
      edge = Edge::BECAME_INACTIVE;
    }
    // First-ever commit landing on the idle state emits no edge -- avoids a
    // spurious report right after boot before any real transition happened.
  }

  outActiveState = _hasStable ? _stableActive : rawActive;
  return edge;
}

bool PinMappingLogic::feedAnalogSample(int rawValue, uint32_t nowMs, float& outValue) {
  if (_hasPolledOnce && (nowMs - _lastPollAt) < _analogInCfg.pollIntervalMs) return false;
  _lastPollAt = nowMs;
  _hasPolledOnce = true;

  const float rawRange = _analogInCfg.rawMax - _analogInCfg.rawMin;
  float scaled = _analogInCfg.scaledMin;
  if (rawRange != 0) {
    const float t = (static_cast<float>(rawValue) - _analogInCfg.rawMin) / rawRange;
    scaled = _analogInCfg.scaledMin + t * (_analogInCfg.scaledMax - _analogInCfg.scaledMin);
  }

  if (_hasReportedOnce) {
    const float delta = scaled - _lastReportedValue;
    const float absDelta = delta < 0 ? -delta : delta;
    if (absDelta < _analogInCfg.changeThreshold) return false;
  }

  _lastReportedValue = scaled;
  _hasReportedOnce = true;
  outValue = scaled;
  return true;
}

uint16_t PinMappingLogic::outputDuty(float commandPercent0to100, uint16_t pwmMin, uint16_t pwmMax, bool activeLow) {
  float pct = commandPercent0to100;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  if (activeLow) pct = 100 - pct;

  const float range = static_cast<float>(pwmMax) - static_cast<float>(pwmMin);
  const float duty = static_cast<float>(pwmMin) + (pct / 100.0f) * range;
  return static_cast<uint16_t>(duty + 0.5f);
}
