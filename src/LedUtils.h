#pragma once
#include "FrequentlyUtils.h"
#include <Arduino.h>

class LEDExpiringToggler {

  // This class toggles a GPIO Pin (uint8_t for ESP32 - style GPIOx) on and off
  // throughout a specified interval. Each time toggleLED() is called, the internal
  // time reference `lastTriggerObservedMilli` is set to the current time milliseconds.
  // Throughout the interval `intervalMs` thereafter, the LED is allowed to be toggled
  // between on and off and when exceeding the interval, the LED is turned off.
  // To produce human-visible blinking, a `toggleIntervalMs` specifies the number of
  // milliseconds after which the output is alternated between on <-> off.
  //
  // This implementation is intended to run on the controller loop, consuming minimal
  // resources. Results should be largely deterministic across different controllers as
  // we don't rely on CPU frequency.

  public:
  LEDExpiringToggler(uint8_t pin, int64_t lifetimeMs, unsigned long toggleIntervalMs, bool highIsOn); // constructor

  void checkToggleLED(); // Loop function

  // Lifecycle functions
  void activate(long delayMs = 0); // activates the LED toggling (after optional delay [milliseconds])
  void expire();                   // disables the LED toggling
  bool isExpired();                // returns true if LED toggling is expired/disabled

  static const bool HIGH_IS_ON; // Indicates that GPIO state HIGH means LED is on
  static const bool LOW_IS_ON;  // Indicates that GPIO state LOW means LED is on (modus operandi for build-in LEDs in Arduino Nano EPS32)

  private:
  void setLedOn();
  void setLedOff();

  // behavioral parameters are lifetime-constants (provided at construction)
  const uint8_t pin;
  const bool highIsOn;

  // dynamic state parameters
  FrequencyToggler toggler;
};