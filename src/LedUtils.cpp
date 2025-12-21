#include "LedUtils.h"
#include <cstdint>     // For int64_t
#include <esp_timer.h> // For esp_timer_get_time()

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ *
 *                                   CLASS LEDExpiringToggler                                     *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

// This class toggles a GPIO Pin (uint8_t for ESP32 - style GPIOx) on and off throughout a specified `lifetimeMs` interval.
// Each time `activate()` is called, the time of the *latest* activation is remembered. Throughout the interval `lifetimeMs`
// thereafter, the LED is allowed to be toggled between on and off. When exceeding the lifetime, the LED is turned off. To
// produce human-visible blinking, a `toggleIntervalMs` specifies the number of milliseconds after which the output is
// alternated between on <-> off.
//
// Internally, all time bookkeeping is done in microseconds for efficiency. All variables representing time
// in microseconds have the suffix 'Micros'. Constructors and activate() take milliseconds as input (unsigned int, with 'Ms'
// suffix) to reflect human-relevant time scales. The different units are also reflected by types:
//   • `unsigned int` represents durations and delays in Milliseconds. Those can only be positive and allow
//      value in the range 0 to 2^32 -1 milliseconds (quite precisely 49 days and 17 hours).
//   •  for lifetime (and internally for bookkeeping), we use `int64_t` , which allows representing both
//      positive and negative times covering large operational time spans beyond 50 days.
//
// There are two checkToggleLED() functions:
//   1. checkToggleLED(int64_t currentMicros): efficient, takes current time in microseconds (recommended for controller loop)
//   2. checkToggleLED(): convenience, but less efficient (calls esp_timer_get_time() internally)
//      For best performance, call esp_timer_get_time() once per loop and pass the value to all instances.
//
// This implementation is intended to run on the controller loop, consuming minimal resources. Results should be
// largely deterministic across different controllers as we don't rely on CPU frequency.

// constructor:
LEDExpiringToggler::LEDExpiringToggler(uint8_t pin, int64_t lifetimeMs, unsigned int toggleIntervalMs, bool highIsOn)
    : pin(pin),
      highIsOn(highIsOn),
      toggler(lifetimeMs, toggleIntervalMs) {
  pinMode(pin, OUTPUT);
  setLedOff_();
}

// Efficient: pass current time in microseconds (recommended)
void LEDExpiringToggler::checkToggleLED(int64_t currentMicros) {
  if (!toggler.checkToggle(currentMicros)) return;
  toggleLED_();
}

// Convenience: calls esp_timer_get_time() internally (less efficient)
void LEDExpiringToggler::checkToggleLED() {
  // we let the internal toggler call `esp_timer_get_time()` instead of _always_ calling it here,
  // because `toggler.checkToggle()` shortcuts the expensive `esp_timer_get_time()` call in various cases
  if (!toggler.checkToggle()) return;
  toggleLED_();
}

void LEDExpiringToggler::toggleLED_() {
  // state has changed, so query new state and set LED accordingly
  if (toggler.isCurrentStateOn()) {
    setLedOn_();
  } else {
    setLedOff_();
  }
}

void LEDExpiringToggler::activate(unsigned int delayMs /* = 0 */) {
  // Calling activate() itself leaves the LED off, but activates the LED toggling cycle (after specified delay).
  // The next call to `checkToggleLED()` (after `delayMs` milliseconds), will turn the LED on.
  // This is exactly how the underlying FrequencyToggler works.
  toggler.activate(delayMs);
}

void LEDExpiringToggler::expire() {
  toggler.expire();
  setLedOff_();
}

bool LEDExpiringToggler::isExpired() { return toggler.isExpired(); }

void LEDExpiringToggler::setLedOn_() {
  if (highIsOn) {
    digitalWrite(pin, HIGH);
  } else {
    digitalWrite(pin, LOW);
  }
}

void LEDExpiringToggler::setLedOff_() {
  if (highIsOn) {
    digitalWrite(pin, LOW);
  } else {
    digitalWrite(pin, HIGH);
  }
}
