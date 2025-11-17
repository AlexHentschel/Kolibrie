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
    : pin(pin), lifetimeMs(lifetimeMs), toggleIntervalMs(static_cast<int64_t>(toggleIntervalMs)), highIsOn(highIsOn) {
  pinMode(pin, OUTPUT);
  setLedOff();
}

void LEDExpiringToggler::checkToggleLED() {
  if (expired) return;
  int64_t currentMillis = esp_timer_get_time() / 1000LL; // convert microseconds returned by `esp_timer_get_time()` to milliseconds
  int64_t sinceActivation = currentMillis - lastActivationObservedMilli;

  // If the lifetime has expired, turn LED to off and mark as expired.
  // note: negative lifetimeMs means no expiration
  if ((lifetimeMs >= 0LL) && (sinceActivation > lifetimeMs)) {
    setLedOff();
    expired = true;
    return;
  }

  // within lifetime, but still before next toggle time: nothing to do
  if (currentMillis < nextToggleAtOrAfterMilli) {
    return;
  }

  // we reached or exceeded the next toggle time:
  // • schedule next trigger time , skip missed intervals
  // • invert LED state
  nextToggleAtOrAfterMilli += toggleIntervalMs;
  while (currentMillis >= nextToggleAtOrAfterMilli) {
    nextToggleAtOrAfterMilli += toggleIntervalMs;
  }
  if (stateIsOn) {
    setLedOff();
  } else {
    setLedOn();
  }
}

void LEDExpiringToggler::activate(long delayMs /* = 0 */) {
  if (lifetimeMs == 0LL) return; // no lifetime, so we don't need to trigger
  lastActivationObservedMilli = esp_timer_get_time() / 1000LL + static_cast<int64_t>(delayMs);
  expired = false;

  // Calling activate() itself leaves the LED off, but activates the LED toggling cycle (after specified delay).
  // The next call to `checkToggleLED()` (after `delayMs` milliseconds), will turn the LED on.
  stateIsOn = false;
  nextToggleAtOrAfterMilli = lastActivationObservedMilli;
}

void LEDExpiringToggler::expire() {
  expired = true;
  setLedOff();
}

bool LEDExpiringToggler::isExpired() {
  return expired;
}

void LEDExpiringToggler::setLedOn() {
  stateIsOn = true;
  if (highIsOn) {
    digitalWrite(pin, HIGH);
  } else {
    digitalWrite(pin, LOW);
  }
}

void LEDExpiringToggler::setLedOff() {
  stateIsOn = false;
  if (highIsOn) {
    digitalWrite(pin, LOW);
  } else {
    digitalWrite(pin, HIGH);
  }
}
