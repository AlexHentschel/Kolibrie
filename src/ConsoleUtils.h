#pragma once
#include <Arduino.h>

class PrintLifeSign {

  // CLASS PrintLifeSign
  //
  // This class prints a life-sign message to the Serial console at specified intervals.
  //
  // The constructor instantiates a _disabled_ printer, which can be enabled by calling `activate()`.
  // Once activated, the trigger will return true on the first call within every time interval
  // of `printIntervalMs` milliseconds. If an inteval is missed (e.g., because the controller loop
  // is busy), the interval is skipped and the trigger will return true on the next interval as it would
  // otherwise.
  // The lifetime is measured from the point of latest activation. After the specified `lifetimeMs`
  // [milliseconds] has elapsed, or `expired()` is called, the trigger deactivates and no longer returns
  // true - until `activate()` is called again.
  // Negative lifetime means that the trigger remains active indefinitely until `expire()` is called.
  //
  // This implementation is intended to run on the controller loop, consuming minimal
  // resources. Results should be largely deterministic across different controllers as
  // we don't rely on CPU frequency.

  public:
  PrintLifeSign(int64_t lifetimeMs, unsigned long printIntervalMs, String message); // constructor

  void checkConsolePrint(); // Loop function

  // Lifecycle functions
  void activate(long delayMs = 0); // activates the life-sign printing (after optional delay [milliseconds])
  void expire();                   // disables the life-sign printing
  bool isExpired();                // returns true if life-sign printing is expired/disabled

  private:
  // behavioral parameters are lifetime-constants (provided at construction)
  const int64_t lifetimeMs;
  const int64_t printIntervalMs;
  const String message;

  // dynamic state parameters
  int64_t lastActivationObservedMilli;
  int64_t nextPrintAtOrAfterMilli;
  bool expired;
};