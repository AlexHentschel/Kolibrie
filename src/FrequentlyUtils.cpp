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
    : frequencyToggler2(lifetimeMs, triggerIntervalMs, triggerIntervalMs) {
}

bool FrequencyToggler::checkToggle() { return frequencyToggler2.checkToggle(); }
bool FrequencyToggler::isCurrentStateOn() { return frequencyToggler2.isCurrentStateOn(); }

void FrequencyToggler::expire() { frequencyToggler2.expire(); }
void FrequencyToggler::activate(long delayMs /* = 0 */) { frequencyToggler2.activate(delayMs); }
bool FrequencyToggler::isExpired() { return frequencyToggler2.isExpired(); }
bool FrequencyToggler::isActive() { return frequencyToggler2.isActive(); }

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ *
 *                                    CLASS FrequencyToggler2                                     *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
// The lifetime is measured from the point of latest activation. After the specified `lifetimeMs`
// [milliseconds] has elapsed, or `expire()` is called, the toggler behaves as follows:
//   * If the state is ON, when the lifetime expires or `expire()` is called, then `checkToggle()` switches
//     the state to OFF on the subsequent call, returning true once.
//   * If the state is OFF, when the lifetime expires or `expire()` is called, no state change occurs, i.e
//     `checkToggle()` returns false.
// Internally, the `checkToggle()` represents three states:
//  * ACTIVE_ON: the toggler is active and in the ON state

// constructor:
FrequencyToggler2::FrequencyToggler2(int64_t lifetimeMs, unsigned long toggleDurationOnMs, unsigned long toggleDurationOffMs)
    : lifetimeMs(lifetimeMs),
      toggleDurationOnMs(static_cast<int64_t>(toggleDurationOnMs)),
      toggleDurationOffMs(static_cast<int64_t>(toggleDurationOffMs)),
      status(_status::Expired),
      stateIsOn(false),
      lastActivationObservedMilli(0),
      nextTriggerAtOrAfterMilli(0) {
}

bool FrequencyToggler2::checkToggle() {
  if (status >= 2) return false;
  int64_t currentMillis = esp_timer_get_time() / 1000LL; // convert microseconds returned by `esp_timer_get_time()` to milliseconds
  int64_t sinceActivation = currentMillis - lastActivationObservedMilli;

  // If the lifetime has expired, mark as expired and return false.
  // note: negative lifetimeMs means no expiration
  if (((lifetimeMs >= 0LL) && (sinceActivation > lifetimeMs)) || (status == _status::ShouldExpire)) {
    // we only want to toggle, if the current state is "on" when the lifetime expires
    bool sendToggleSignal = stateIsOn;
    status = _status::Expired;
    stateIsOn = false;
    return sendToggleSignal;
  }

  // within lifetime, but still before next trigger time: nothing to do
  if (currentMillis < nextTriggerAtOrAfterMilli) {
    return false;
  }

  // we reached or exceeded the next trigger time:
  // • schedule next trigger time, skip missed intervals
  // • and return true
  advanceState(currentMillis);
  return true;
}

bool FrequencyToggler2::isCurrentStateOn() {
  return stateIsOn;
}

void FrequencyToggler2::activate(long delayMs /* = 0 */) {
  if (lifetimeMs == 0LL) return; // no lifetime, so we don't need to trigger
  lastActivationObservedMilli = esp_timer_get_time() / 1000LL + static_cast<int64_t>(delayMs);
  status = _status::Active;

  // trigger on next call to `checkTrigger()` (after `delayMs` milliseconds)
  nextTriggerAtOrAfterMilli = lastActivationObservedMilli;
}

void FrequencyToggler2::expire() {
  if (status != _status::Expired)
    status = _status::ShouldExpire;
}

bool FrequencyToggler2::isExpired() { return !isActive(); }

bool FrequencyToggler2::isActive() { return status == _status::Active; }

void FrequencyToggler2::advanceState(int64_t currentMillis) {
  if (currentMillis < nextTriggerAtOrAfterMilli) return;

  do {
    int64_t accumulatedAdvanceMs = 0LL;

    for (int i = 0; i < 2; i++) {
      stateIsOn = !stateIsOn; // updated state; persists for the respective toggle duration
      int64_t deltaMs = stateIsOn ? toggleDurationOnMs : toggleDurationOffMs;
      nextTriggerAtOrAfterMilli += deltaMs;
      accumulatedAdvanceMs += deltaMs;
      if (currentMillis < nextTriggerAtOrAfterMilli) return;
    }
    // If we reach the following code, then the following holds:
    // • currentMillis >= nextTriggerAtOrAfterMilli, so we need to advance further
    // • we have toggled twice, i.e. the value of `stateIsOn` is where it started
    //   and we accumulatedAdvanceMs = toggleDurationOnMs + toggleDurationOffMs.
    // We now attempt to advance by another two toggles in one step, leaving the boolean status of `stateIsOn` invariant. In total,
    // we have then advanced by accumulatedAdvanceMs = 2 * (toggleDurationOnMs + toggleDurationOffMs). Subsequently, we attempt to
    // add the updated accumulatedAdvanceMs again, yielding a total updated advance of 4 * (toggleDurationOnMs + toggleDurationOffMs).
    //
    // This is an exponential growth, which eventually is going to overshoot. We remember the value before the last advancement, which
    // by construction is guaranteed to be less than currentMillis. Then, we restart the proceed from the start.

    for (int64_t speculativeExponentialAdvanceMs = nextTriggerAtOrAfterMilli + accumulatedAdvanceMs;
         currentMillis > speculativeExponentialAdvanceMs;
         accumulatedAdvanceMs <<= 1) {
      nextTriggerAtOrAfterMilli = speculativeExponentialAdvanceMs;
    }

    // At this point, we _always_ have currentMillis  <= nextTriggerAtOrAfterMilli, so we sill need to advance further.
    // However, as our last exponential step overshot, we now restart again by adding the minimal increments

  } while (true);
}
