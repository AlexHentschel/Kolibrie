#include "FrequentlyUtils.h"
#include <cstdint>     // For int64_t
#include <esp_timer.h> // For esp_timer_get_time()

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ *
 *                                    CLASS FrequencyTrigger                                      *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
// This class provides a boolean trigger that returns true _once_ in specified time intervals.
// It is intended to run on the controller loop, consuming minimal resources.

// constructor:
FrequencyTrigger::FrequencyTrigger(int64_t lifetimeMs, unsigned long triggerIntervalMs)
    : lifetimeMs(lifetimeMs),
      triggerIntervalMs(static_cast<int64_t>(triggerIntervalMs)),
      lastActivationObservedMilli(0),
      nextTriggerAtOrAfterMilli(0),
      expired(true) // start as expired/disabled
{}

bool FrequencyTrigger::checkTrigger() {
  if (expired) return false;
  int64_t currentMillis = esp_timer_get_time() / 1000LL; // convert microseconds returned by `esp_timer_get_time()` to milliseconds
  int64_t sinceActivation = currentMillis - lastActivationObservedMilli;

  // If the lifetime has expired, mark as expired and return false.
  // note: negative lifetimeMs means no expiration
  if ((lifetimeMs >= 0LL) && (sinceActivation > lifetimeMs)) {
    expired = true;
    return false;
  }

  // within lifetime, but still before next trigger time: nothing to do
  if (currentMillis < nextTriggerAtOrAfterMilli) {
    return false;
  }

  // we reached or exceeded the next trigger time:
  // • schedule next trigger time, skip missed intervals
  // • and return true
  nextTriggerAtOrAfterMilli += triggerIntervalMs;
  while (currentMillis >= nextTriggerAtOrAfterMilli) {
    nextTriggerAtOrAfterMilli += triggerIntervalMs;
  }
  return true;
}

void FrequencyTrigger::activate(long delayMs /* = 0 */) {
  if (lifetimeMs == 0LL) return; // no lifetime, so we don't need to trigger
  lastActivationObservedMilli = esp_timer_get_time() / 1000LL + static_cast<int64_t>(delayMs);
  expired = false;

  // trigger on next call to `checkTrigger()` (after `delayMs` milliseconds)
  nextTriggerAtOrAfterMilli = lastActivationObservedMilli;
}

void FrequencyTrigger::expire() { expired = true; }

bool FrequencyTrigger::isExpired() { return expired; }

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ *
 *                                    CLASS FrequencyToggler                                      *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
// This class provides an on/off toggling that changes between the on- and off-state _once_ in
// specified time intervals.

// constructor:
FrequencyToggler::FrequencyToggler(int64_t lifetimeMs, unsigned long triggerIntervalMs)
    : frequencyTrigger(lifetimeMs, triggerIntervalMs) {
  stateIsOn = false;
}

bool FrequencyToggler::checkToggle() {
  bool stateChange = frequencyTrigger.checkTrigger();
  if (stateChange) {
    stateIsOn = !stateIsOn;
  }
  return stateChange;
}

bool FrequencyToggler::isCurrentStateOn() {
  return stateIsOn;
}

void FrequencyToggler::expire() {
  frequencyTrigger.expire();
  stateIsOn = false;
}

void FrequencyToggler::activate(long delayMs /* = 0 */) { frequencyTrigger.activate(delayMs); }
bool FrequencyToggler::isExpired() { return frequencyTrigger.isExpired(); }
