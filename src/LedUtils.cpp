#include "LedUtils.h"
#include <cstdint>     // For int64_t
#include <esp_timer.h> // For esp_timer_get_time()

// CLASS LEDExpiringToggler
//
// This class toggles a GPIO Pin (uint8_t for ESP32 - style GPIOx) on and off
// throughout a specified interval. Each time `activate()` is called, the internal
// time reference `lastActivationObservedMilli` is set to the current time milliseconds.
// Throughout the interval `lifetimeMs` thereafter, the LED is allowed to be toggled
// between on and off and when exceeding the interval, the LED is turned off.
// To produce human-visible blinking, a `toggleIntervalMs` specifies the number of
// milliseconds after which the output is alternated between on <-> off.
//
// The constructor instantiates a _disabled_ toggler, which must be enabled by calling
// `activate()` for blinking to start. Once activated, the LED will start the blinking
// cycle in the ON state. After the specified `lifetimeMs` [milliseconds] has elapsed,
// or `expired()` is called, the trigger deactivates and no longer returns true - until
// `activate()` is called again.
// Negative lifetime means that the trigger remains active indefinitely until `expire()`
// is called.
//
// This implementation is intended to run on the controller loop, consuming minimal
// resources. Results should be largely deterministic across different controllers as
// we don't rely on CPU frequency.

const bool LEDExpiringToggler::HIGH_IS_ON = true;
const bool LEDExpiringToggler::LOW_IS_ON = false;

// constructor:
LEDExpiringToggler::LEDExpiringToggler(uint8_t pin, int64_t lifetimeMs, unsigned long toggleIntervalMs, bool highIsOn)
    : pin(pin),
      highIsOn(highIsOn),
      toggler(lifetimeMs, toggleIntervalMs) {
  pinMode(pin, OUTPUT);
  setLedOff();
}

void LEDExpiringToggler::checkToggleLED() {
  if (!toggler.checkToggle()) return; // also false for

  // state has changed, so query new state and set LED accordingly
  if (toggler.isCurrentStateOn()) {
    setLedOn();
  } else {
    setLedOff();
  }
}

void LEDExpiringToggler::activate(long delayMs /* = 0 */) {
  // Calling activate() itself leaves the LED off, but activates the LED toggling cycle (after specified delay).
  // The next call to `checkToggleLED()` (after `delayMs` milliseconds), will turn the LED on.
  // This is exactly how the underlying FrequencyToggler works.
  toggler.activate(delayMs);
}

void LEDExpiringToggler::expire() {
  toggler.expire();
  setLedOff();
}

bool LEDExpiringToggler::isExpired() { return toggler.isExpired(); }

void LEDExpiringToggler::setLedOn() {
  if (highIsOn) {
    digitalWrite(pin, HIGH);
  } else {
    digitalWrite(pin, LOW);
  }
}

void LEDExpiringToggler::setLedOff() {
  if (highIsOn) {
    digitalWrite(pin, LOW);
  } else {
    digitalWrite(pin, HIGH);
  }
}
