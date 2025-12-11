#pragma once
#include "FrequentlyUtils.h"
#include <Arduino.h>

class PrintLifeSign {

  // CLASS PrintLifeSign
  //
  // This class prints a life-sign message to the Serial console at specified intervals.
  //
  // The constructor instantiates a _disabled_ printer, which can be enabled by calling `activate()`.
  // Once activated, the trigger will print the message on the first call within every time interval
  // of `printIntervalMs` milliseconds. If an interval is missed (e.g., because the controller loop
  // is busy), the interval is skipped and the message will be printed on the next interval as it would
  // otherwise.
  // The lifetime is measured from the point of latest activation. After the specified `lifetimeMs`
  // [milliseconds] has elapsed, or `expire()` is called, the printer deactivates and no longer prints
  // until `activate()` is called again. Negative lifetime means that the printer remains active indefinitely
  // until `expire()` is called.
  //
  // All internal time bookkeeping is done in microseconds for efficiency. All variables representing time
  // have the suffix 'Micros'. Constructors and activate() take milliseconds as input (unsigned int, with 'Ms' suffix)
  // to reflect human-relevant time scales. For best performance, call esp_timer_get_time() once per controller loop
  // and pass the value to all instances. The function without time input is less efficient, as it calls esp_timer_get_time() internally.

  public:
  PrintLifeSign(int64_t lifetimeMs, unsigned int printIntervalMs, String message);

  // Efficient: pass current time in microseconds
  void checkConsolePrint(int64_t currentMicros);
  // Convenience: calls esp_timer_get_time() internally (less efficient)
  void checkConsolePrint();

  // Lifecycle functions
  void activate(unsigned int delayMs = 0); // activates the life-sign printing (after optional delay [milliseconds])
  void expire();                           // disables the life-sign printing
  bool isExpired();                        // returns true if life-sign printing is expired/disabled

  private:
  FrequencyTrigger trigger;
  const String message;
};