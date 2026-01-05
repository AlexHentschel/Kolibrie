#include "FrequentlyUtils.h"
#include <cstdint>     // For int64_t
#include <esp_timer.h> // For esp_timer_get_time()

// TODO: !!
// Integer divisions are slow. This file now uses microseconds for all internal time bookkeeping, avoiding division by 1000.
// Constructors and activate() still take milliseconds for compatibility, but all internal logic is in microseconds.
// For best performance, call esp_timer_get_time() once per controller loop and pass the value to all check...() calls.

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ *
 *                                    CLASS FrequencyTrigger                                      *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
// This class provides a boolean trigger that returns true _once_ in specified time intervals.
// It is intended to run on the controller loop, consuming minimal resources.

// constructor:

FrequencyTrigger::FrequencyTrigger(int64_t lifetimeMs, unsigned int triggerIntervalMs)
    : lifetimeMicros((lifetimeMs < 0LL) ? -1LL : lifetimeMs * 1000LL),
      triggerIntervalMicros(static_cast<int64_t>(triggerIntervalMs) * 1000LL),
      lastActivationObservedMicros(0),
      nextTriggerAtOrAfterMicros(0),
      expired(true) // start as expired/disabled
{}

bool FrequencyTrigger::checkTrigger(int64_t currentMicros) {
  if (expired) return false;
  return checkTrigger_(currentMicros);
}

// Less efficient: calls esp_timer_get_time() internally
bool FrequencyTrigger::checkTrigger() {
  if (expired) return false;
  return checkTrigger_(esp_timer_get_time());
}

bool FrequencyTrigger::checkTrigger_(int64_t currentMicros) {
  int64_t sinceActivation = currentMicros - lastActivationObservedMicros;
  // If the lifetime has expired, mark as expired and return false.
  // note: negative lifetimeMicros means no expiration
  if ((lifetimeMicros >= 0LL) && (sinceActivation > lifetimeMicros)) {
    expired = true;
    return false;
  }
  // within lifetime, but still before next trigger time: nothing to do
  if (currentMicros < nextTriggerAtOrAfterMicros) {
    return false;
  }
  // we reached or exceeded the next trigger time:
  // • schedule next trigger time, skip missed intervals
  // • and return true
  advanceState(currentMicros);
  return true;
}

// advances the internal threshold `nextTriggerAtOrAfterMicros` for next state change.
// The algorithm effectively fast-forwards through missed intervals.
// After `advanceState` returns, the `nextTriggerAtOrAfterMicros` is set to the closest _upcoming_
// time had the algorithm be run more frequently. The implementation efficiently handles large time
// jumps δ (eg. when the controller loop is busy) requiring only O(log(δ)) operations.
//
// Note: According to https://github.com/wled/WLED/issues/4206
// integer divisions, especially 64bit ones can be very slow on microcontrollers (ESP32 S2, S3, C3).
// Therefore, we avoid the integer division and instead utilize a heuristic that is very fast on the
// most common usage pattern of no missed intervals, and still efficient (O(log(δ))) for large jumps δ.
void FrequencyTrigger::advanceState(int64_t currentMicros) {
  if (currentMicros < nextTriggerAtOrAfterMicros) return;
  do {
    nextTriggerAtOrAfterMicros += triggerIntervalMicros;
    if (currentMicros < nextTriggerAtOrAfterMicros) return;
    int64_t accumulatedAdvanceMicros = triggerIntervalMicros;

    // If we reach the following code, then the following holds:
    // • currentMicros >= nextTriggerAtOrAfterMicros, so we need to advance further
    // • accumulatedAdvanceMicros = triggerIntervalMicros.
    // We now attempt to advance by another triggerIntervalMicros. In total, we have then advanced by accumulatedAdvanceMicros = 2 * triggerIntervalMicros.
    // Subsequently, we attempt to add the updated accumulatedAdvanceMicros again, yielding a total advance of 4 * triggerIntervalMicros.
    //
    // This is an exponential growth, which eventually is going to overshoot. We remember the value before the last advancement, which
    // by construction is guaranteed to be less than currentMicros. Then, we restart the process from the last point we have not overshot.

    int64_t speculativeNextTrigger = nextTriggerAtOrAfterMicros + accumulatedAdvanceMicros;
    while (currentMicros > speculativeNextTrigger) {
      // We want to fing the smallest value of `nextTriggerAtOrAfterMicros` such that currentMicros < nextTriggerAtOrAfterMicros holds.
      // This is true for `speculativeNextTrigger`, so we can increase `nextTriggerAtOrAfterMicros` to at least this value.
      nextTriggerAtOrAfterMicros = speculativeNextTrigger;

      // Compute next speculative next trigger time, by doubling the accumulated advance and adding it to the current value of `nextTriggerAtOrAfterMicros`.
      accumulatedAdvanceMicros <<= 1;
      speculativeNextTrigger = nextTriggerAtOrAfterMicros + accumulatedAdvanceMicros;
    }

    // At this point, we have currentMicros >= nextTriggerAtOrAfterMicros, so we may still need to advance further.
    // However, as our last speculative exponential step overshot, we now restart by adding the minimal increments.
  } while (true);
}

void FrequencyTrigger::activate(unsigned int delayMs /* = 0 */) {
  if (lifetimeMicros == 0LL) return; // no lifetime, so we don't need to trigger
  activate_(esp_timer_get_time(), delayMs);
}

void FrequencyTrigger::activate(int64_t currentMicros, unsigned int delayMs) {
  if (lifetimeMicros == 0LL) return; // no lifetime, so we don't need to trigger
  activate_(currentMicros, delayMs);
}

// activate_ is an internal helper that sets up the activation state.
// CAUTION: it should be called for positive `lifetimeMicros` only.
void FrequencyTrigger::activate_(int64_t currentMicros, unsigned int delayMs) {
  lastActivationObservedMicros = currentMicros;
  if (delayMs > 0) {
    lastActivationObservedMicros += static_cast<int64_t>(delayMs) * 1000LL;
  }

  expired = false;
  // trigger on next call to `checkTrigger()` (after `delayMs` milliseconds)
  nextTriggerAtOrAfterMicros = lastActivationObservedMicros;
}

void FrequencyTrigger::expire() { expired = true; }

bool FrequencyTrigger::isExpired() { return expired; }

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ *
 *                                    CLASS CooldownTriggerN                                       *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
// This class provides a boolean trigger that triggers exactly N times. There is a cooldown period
// between subsequent triggers. After N triggers have fired, the trigger expires and no longer
// returns true - until `activate()` is called again to reset the trigger.

// constructor:

CooldownTriggerN::CooldownTriggerN(int n, unsigned int cooldownMs)
    : n(n),
      cooldownMicros(static_cast<int64_t>(cooldownMs) * 1000LL),
      remainingTriggers(0), // start as expired/disabled
      nextTriggerAtOrAfterMicros(0) {}

bool CooldownTriggerN::checkTrigger(int64_t currentMicros) {
  if (remainingTriggers == 0) return false;                     // expired/disabled
  if (currentMicros < nextTriggerAtOrAfterMicros) return false; // Still in cooldown/initial delay period
  advanceState(currentMicros);
  return true;
}

// Less efficient: calls esp_timer_get_time() internally
bool CooldownTriggerN::checkTrigger() {
  if (remainingTriggers == 0) return false; // expired/disabled
  int64_t currentMicros = esp_timer_get_time();
  if (currentMicros < nextTriggerAtOrAfterMicros) return false; // Still in cooldown/initial delay period
  advanceState(currentMicros);
  return true;
}

// CAUTION: this method should only be called if the trigger is just fired. It will update the internal state
// to the next trigger time. Prerequisites:
//   * `remainingTriggers ≠ 0`, otherwise the trigger is expired/disabled and this method should not be called.
//   * `currentMicros ≥ nextTriggerAtOrAfterMicros`, otherwise the trigger is still in the cooldown/initial delay
//      period and this method should not be called.
void CooldownTriggerN::advanceState(int64_t currentMicros) {
  // Per API contract, the trigger is just fired, so we advance the trigger time by the cooldown period:
  nextTriggerAtOrAfterMicros = currentMicros + cooldownMicros;

  // Possible value ranges for `remainingTriggers`:
  //  * Positive `n` means that we have a finite number of triggers. The initial value of `remainingTriggers` at the time of activation
  //    is set to `n`. For each trigger fired, we decrement `remainingTriggers` by 1. When `remainingTriggers` reaches 0, the trigger
  //    reaches the expired state automatically and this function should not be called anymore. We emphasize that this method will never
  //    change the `remainingTriggers` value to a negative number.
  //  * Zero-valued `remainingTriggers`: this indicates that the trigger is expired and this method should not be called.
  //  * Negative `remainingTriggers`: means infinite number of triggers. This is because the initial value of `remainingTriggers` at the
  //    time of activation is set to `n`. In other words, for negative `n` (infinite number of triggers), `remainingTriggers` is also
  //    negative. We just leave `remainingTriggers` unchanged, because we don't need to count how many triggers remain to be fired.
  // In summary, this method only decrements `remainingTriggers` as long as it is positive.
  if (remainingTriggers > 0) {
    remainingTriggers--;
  }
  return;
}

void CooldownTriggerN::activate(int64_t currentMicros, unsigned int delayMs) {
  if (n == 0) return; // zero means no triggers to fire, i.e. we remain disabled
  activate_(currentMicros, delayMs);
}

void CooldownTriggerN::activate(unsigned int delayMs /* = 0 */) {
  if (n == 0) return; // zero means no triggers to fire, i.e. we remain disabled
  activate_(esp_timer_get_time(), delayMs);
}

// activate_ is an internal helper that sets up the activation state.
// CAUTION: it should be called for positive `lifetimeMicros` only.
void CooldownTriggerN::activate_(int64_t currentMicros, unsigned int delayMs) {
  remainingTriggers = n; // negative means infinite
  nextTriggerAtOrAfterMicros = currentMicros;
  if (delayMs > 0) {
    nextTriggerAtOrAfterMicros += static_cast<int64_t>(delayMs) * 1000LL;
  }
}

void CooldownTriggerN::expire() { remainingTriggers = 0; }

bool CooldownTriggerN::isExpired() { return remainingTriggers == 0; }

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ *
 *                                    CLASS FrequencyToggler                                      *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
// This class provides an on/off toggling that changes between the on- and off-state _once_ in
// specified time intervals.

// constructor:
FrequencyToggler::FrequencyToggler(int64_t lifetimeMs, unsigned int toggleIntervalMs)
    : frequencyToggler2_(lifetimeMs, toggleIntervalMs, toggleIntervalMs) {}

bool FrequencyToggler::checkToggle(int64_t currentMicros) { return frequencyToggler2_.checkToggle(currentMicros); }
bool FrequencyToggler::checkToggle() { return frequencyToggler2_.checkToggle(); }
bool FrequencyToggler::isCurrentStateOn() { return frequencyToggler2_.isCurrentStateOn(); }

void FrequencyToggler::expire() { frequencyToggler2_.expire(); }
void FrequencyToggler::activate(unsigned int delayMs /* = 0 */) { frequencyToggler2_.activate(delayMs); }
void FrequencyToggler::activate(int64_t currentMicros, unsigned int delayMs) { frequencyToggler2_.activate(currentMicros, delayMs); }
bool FrequencyToggler::isExpired() { return frequencyToggler2_.isExpired(); }
bool FrequencyToggler::isActive() { return frequencyToggler2_.isActive(); }

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

FrequencyToggler2::FrequencyToggler2(int64_t lifetimeMs, unsigned int toggleDurationOnMs, unsigned int toggleDurationOffMs)
    : lifetimeMicros((lifetimeMs < 0LL) ? -1LL : lifetimeMs * 1000LL),
      toggleDurationOnMicros(static_cast<int64_t>(toggleDurationOnMs) * 1000LL),
      toggleDurationOffMicros(static_cast<int64_t>(toggleDurationOffMs) * 1000LL),
      status(_status::Expired),
      stateIsOn(false),
      lastActivationObservedMicros(0),
      nextTriggerAtOrAfterMicros(0) {}

bool FrequencyToggler2::checkToggle(int64_t currentMicros) {
  if (status >= 2) return false;
  return checkToggle_(currentMicros);
}

// Less efficient: calls esp_timer_get_time() internally
bool FrequencyToggler2::checkToggle() {
  if (status >= 2) return false;
  return checkToggle_(esp_timer_get_time());
}

bool FrequencyToggler2::checkToggle_(int64_t currentMicros) {
  int64_t sinceActivation = currentMicros - lastActivationObservedMicros;
  // If the lifetime has expired, mark as expired and inform the caller whether the state has changed from on->off.
  // note: negative lifetimeMicros means no expiration
  if (((lifetimeMicros >= 0LL) && (sinceActivation > lifetimeMicros)) || (status == _status::ShouldExpire)) {
    // We only want to toggle, if the current state is "on" when the lifetime expires.
    // Otherwise, we just quietly remain in the off state, but set out internal state to expired.
    bool sendToggleSignal = stateIsOn;
    status = _status::Expired;
    stateIsOn = false;
    return sendToggleSignal;
  }

  // within lifetime, but still before next trigger time: nothing to do
  if (currentMicros < nextTriggerAtOrAfterMicros) {
    return false;
  }

  // we reached or exceeded the next trigger time:
  // • schedule next trigger time, skip missed intervals
  // • and return true
  advanceState(currentMicros);
  return true;
}

bool FrequencyToggler2::isCurrentStateOn() {
  return stateIsOn;
}

void FrequencyToggler2::activate(unsigned int delayMs /* = 0 */) {
  if (lifetimeMicros == 0LL) return; // no lifetime, so we don't need to trigger
  activate_(esp_timer_get_time(), delayMs);
}

void FrequencyToggler2::activate(int64_t currentMicros, unsigned int delayMs) {
  if (lifetimeMicros == 0LL) return; // no lifetime, so we don't need to trigger
  activate_(currentMicros, delayMs);
}

// activate_ is an internal helper that sets up the activation state.
// CAUTION: it should be called for positive `lifetimeMicros` only.
void FrequencyToggler2::activate_(int64_t currentMicros, unsigned int delayMs) {
  status = _status::Active;
  lastActivationObservedMicros = currentMicros;
  if (delayMs > 0) {
    lastActivationObservedMicros += static_cast<int64_t>(delayMs) * 1000LL;
  }
  // trigger on next call to `checkTrigger()` (after `delayMs` milliseconds)
  nextTriggerAtOrAfterMicros = lastActivationObservedMicros;
}

bool FrequencyToggler2::isExpired() { return !isActive(); }

bool FrequencyToggler2::isActive() { return status == _status::Active; }

// advances the internal threshold `nextTriggerAtOrAfterMicros` for next state change.
// The algorithm effectively fast-forwards through missed intervals, toggling the state accordingly.
// After `advanceState` returns, the `nextTriggerAtOrAfterMicros` is set to the closest _upcoming_
// toggle time. The implementation efficiently handles large time jumps δ (eg. when the controller
// loop is busy) requiring only O(log(δ)) operations.
//
// Note: According to https://github.com/wled/WLED/issues/4206
// integer divisions, especially 64bit ones can be very slow on microcontrollers (ESP32 S2, S3, C3).
// Therefore, we avoid the integer division and instead utilize a heuristic that is very fast on the
// most common usage pattern of no missed intervals, and still efficient (O(log(δ))) for large jumps δ.
void FrequencyToggler2::advanceState(int64_t currentMicros) {
  if (currentMicros < nextTriggerAtOrAfterMicros) return;
  do {
    int64_t accumulatedAdvanceMicros = 0LL;
    for (int i = 0; i < 2; i++) {
      stateIsOn = !stateIsOn; // updated state; persists for the respective toggle duration
      int64_t deltaMicros = stateIsOn ? toggleDurationOnMicros : toggleDurationOffMicros;
      nextTriggerAtOrAfterMicros += deltaMicros;
      accumulatedAdvanceMicros += deltaMicros;
      if (currentMicros < nextTriggerAtOrAfterMicros) return;
    }

    // If we reach the following code, then the following holds:
    // • currentMicros >= nextTriggerAtOrAfterMicros, so we need to advance further
    // • we have toggled twice, i.e. the value of `stateIsOn` is where it started
    //   and we accumulatedAdvanceMicros = toggleDurationOnMicros + toggleDurationOffMicros.
    // We now attempt to advance by another two toggles in one step, leaving the boolean status of `stateIsOn` invariant. In total,
    // we have then advanced by accumulatedAdvanceMicros = 2 * (toggleDurationOnMicros + toggleDurationOffMicros). Subsequently, we attempt to
    // add the updated accumulatedAdvanceMicros again, yielding a total advance of 4 * (toggleDurationOnMicros + toggleDurationOffMicros).
    //
    // This is an exponential growth, which eventually is going to overshoot. We remember the value before the last advancement, which
    // by construction is guaranteed to be less than currentMicros. Then, we restart the proceed from the last point we have not overshot.

    int64_t speculativeNextTrigger = nextTriggerAtOrAfterMicros + accumulatedAdvanceMicros;
    while (currentMicros > speculativeNextTrigger) {
      // We want to fing the smallest value of `nextTriggerAtOrAfterMicros` such that currentMicros < nextTriggerAtOrAfterMicros holds.
      // This is true for `speculativeNextTrigger`, so we can increase `nextTriggerAtOrAfterMicros` to at least this value.
      nextTriggerAtOrAfterMicros = speculativeNextTrigger;

      // Compute next speculative next trigger time, by doubling the accumulated advance and adding it to the current value of `nextTriggerAtOrAfterMicros`.
      accumulatedAdvanceMicros <<= 1;
      speculativeNextTrigger = nextTriggerAtOrAfterMicros + accumulatedAdvanceMicros;
    }

    // At this point, we have currentMicros >= nextTriggerAtOrAfterMicros, so we may still need to advance further.
    // However, as our last speculative exponential step overshot, we now restart by adding the minimal increments.
  } while (true);
}
