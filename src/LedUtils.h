#pragma once
#include "FrequentlyUtils.h"
#include <Arduino.h>

namespace LedUtils {
  constexpr bool HIGH_IS_ON = true;
  constexpr bool LOW_IS_ON = false;
}

class LEDExpiringToggler {

  // CLASS LEDExpiringToggler
  //
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

  public:
  // Constructor: all time arguments in milliseconds (unsigned int, with 'Ms' suffix)
  LEDExpiringToggler(uint8_t pin, int64_t lifetimeMs, unsigned int toggleIntervalMs, bool highIsOn);

  // Efficient: pass current time in microseconds (recommended)
  void checkToggleLED(int64_t currentMicros);
  // Convenience: calls esp_timer_get_time() internally (less efficient)
  void checkToggleLED();

  // Lifecycle functions
  void activate(unsigned int delayMs = 0); // activates the LED toggling (after optional delay [milliseconds])
  void expire();                           // disables the LED toggling
  bool isExpired();                        // returns true if LED toggling is expired/disabled

  private:
  void toggleLED_();
  void setLedOn_();
  void setLedOff_();

  // behavioral parameters are lifetime-constants (provided at construction)
  const uint8_t pin;
  const bool highIsOn;

  // dynamic state parameters
  FrequencyToggler toggler;
};

class LEDExpiringToggler2 {

  // CLASS LEDExpiringToggler2
  //
  // This class toggles a GPIO Pin (uint8_t for ESP32 - style GPIOx) on and off throughout a specified `lifetimeMs` interval.
  // The LEDExpiringToggler2 supports asymmetric blinking patterns (e.g., short flash followed by longer pause), by
  // specifying separate durations for the ON and OFF states.
  //
  // Each time `activate()` is called, the time of the *latest* activation is remembered. Throughout the interval `lifetimeMs`
  // thereafter, the LED is allowed to be toggled between on and off. When exceeding the lifetime, the LED is turned off. To
  // produce human-visible blinking, `toggleDurationOnMs` and `toggleDurationOffMs` specify the number of milliseconds the
  // LED stays on and off, respectively.
  //
  // Internally, all time bookkeeping is done in microseconds for efficiency. All variables representing time
  // in microseconds have the suffix 'Micros'. Constructors and activate() take milliseconds as input (unsigned int, with 'Ms'
  // suffix) to reflect human-relevant time scales. The different units are also reflected by types:
  //   • `unsigned int` represents durations and delays in Milliseconds. Those can only be positive and allow
  //      value in the range 0 to 2^32 -1 milliseconds (quite precisely 49 days and 17 hours).
  //   •  for lifetime (and internal bookkeeping), we use `int64_t` , which allows representing both
  //      positive and negative times covering large operational time spans beyond 50 days.
  //
  // There are two checkToggleLED() functions:
  //   1. checkToggleLED(int64_t currentMicros): efficient, takes current time in microseconds (recommended for controller loop)
  //   2. checkToggleLED(): convenience, but less efficient (calls esp_timer_get_time() internally)
  //      For best performance, call esp_timer_get_time() once per loop and pass the value to all instances.
  //
  // This implementation is intended to run on the controller loop, consuming minimal resources. Results should be
  // largely deterministic across different controllers as we don't rely on CPU frequency.

  public:
  // Constructor with separate on/off durations: all time arguments in milliseconds (unsigned int, with 'Ms' suffix)
  LEDExpiringToggler2(uint8_t pin, int64_t lifetimeMs, unsigned int toggleDurationOnMs, unsigned int toggleDurationOffMs, bool highIsOn);
  
  // Constructor with single interval (backward compatibility with LEDExpiringToggler): sets both on and off durations to the same value
  LEDExpiringToggler2(uint8_t pin, int64_t lifetimeMs, unsigned int toggleIntervalMs, bool highIsOn);

  // Efficient: pass current time in microseconds (recommended)
  void checkToggleLED(int64_t currentMicros);
  // Convenience: calls esp_timer_get_time() internally (less efficient)
  void checkToggleLED();

  // Lifecycle functions
  void activate(unsigned int delayMs = 0); // activates the LED toggling (after optional delay [milliseconds])
  void expire();                           // disables the LED toggling
  bool isExpired();                        // returns true if LED toggling is expired/disabled

  private:
  void toggleLED_();
  void setLedOn_();
  void setLedOff_();

  // behavioral parameters are lifetime-constants (provided at construction)
  const uint8_t pin;
  const bool highIsOn;

  // dynamic state parameters
  FrequencyToggler2 toggler;
};