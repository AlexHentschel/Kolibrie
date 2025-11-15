#include "LedUtils.h"

// CLASS LEDExpiringToggler
//
// This class toggles a GPIO Pin (uint8_t for ESP32 - style GPIOx) on and off
// throughout a specified interval. Each time toggleLED() is called, the internal
// time reference `lastTriggerObservedMilli` is set to the current time milliseconds.
// Throughout the interval `intervalMs` thereafter, the LED is allowed to be toggled
// between on and off and when exceeding the interval, the LED is turned off.
// To produce human-visible blinking, a `toggleIntervalMs` specifies the number of
// milliseconds after which the output is alternated between on <-> off.
//
// This is intended to run on the controller loop, consuming minimal resources.
// Results should be largely deterministic across different controllers as we
// don't rely on CPU frequency

const bool LEDExpiringToggler::HIGH_IS_ON = true;
const bool LEDExpiringToggler::LOW_IS_ON = false;

// constructor:
LEDExpiringToggler::LEDExpiringToggler(uint8_t pin, long lifetimeMs, unsigned long toggleIntervalMs, bool highIsOn)
    : pin(pin), lifetimeMs(lifetimeMs), toggleIntervalMs(toggleIntervalMs), lastTriggerObservedMilli(0), highIsOn(highIsOn) {
  pinMode(pin, OUTPUT);
  setLedOff();
}

void LEDExpiringToggler::checkToggleLED() {
  if (expired) return;
  unsigned long currentMillis = millis();
  unsigned long sinceTrigger = currentMillis - lastTriggerObservedMilli;

  // note: negative lifetimeMs indicates infinite lifetime
  if ((lifetimeMs >= 0) && (sinceTrigger > lifetimeMs)) {
    setLedOff();
    expired = true;
    return;
  }

  // still active within lifetime:
  // We divide `sinceTrigger` by toggleIntervalMs, which is essentially a zero-based counter of
  // the elapsed toggle intervals. We always start with HIGH, on the zero value of the counter.
  // Taking module 2 of the counter yields 0 for even or 1 for odd values. Per convention, we start
  // with HIGH on the always start with HIGH during the immediate toggleIntervalMs following a
  // trigger -- corresponding to counter value 0.
  unsigned long counter = sinceTrigger / toggleIntervalMs;
  // modulo 2 is implemented below as bitwise AND with 1. We utilize that the least
  // significant bit of a binary number determines if it is even (0) or odd (1).
  if ((counter & 1) == 0) {
    setLedOn();
  } else {
    setLedOff();
  }
}

void LEDExpiringToggler::trigger() {
  if (lifetimeMs == 0) return; // no lifetime, so we don't need to trigger
  lastTriggerObservedMilli = millis();
  expired = false;
  setLedOn();
}

void LEDExpiringToggler::expire() {
  expired = true;
  setLedOff();
}

bool LEDExpiringToggler::isExpired() {
  return expired;
}

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
