#pragma once
#include <Arduino.h>

namespace FrequencyUtils {
  constexpr int64_t unbounded_lifetime = -1LL;
}

class FrequencyTrigger {

  // CLASS FrequencyTrigger
  //
  // This class provides a boolean trigger that returns true _once_ in specified time intervals.
  //
  // The constructor instantiates a _disabled_ trigger, which can be enabled by calling `activate()`.
  // Once activated, the trigger will return true on the first call within every time interval
  // of `triggerIntervalMs` milliseconds. If an inteval is missed (e.g., because the controller loop
  // is busy), the interval is skipped and the trigger will return true on the next interval as it would
  // otherwise.
  // The lifetime is measured from the point of latest activation. After the specified `lifetimeMs`
  // [milliseconds] has elapsed, or `expire()` is called, the trigger deactivates and no longer returns
  // true - until `activate()` is called again.
  // Negative lifetime means that the trigger remains active indefinitely until `expire()` is called.
  //
  // This implementation is intended to run on the controller loop, consuming minimal
  // resources. Results should be largely deterministic across different controllers as
  // we don't rely on CPU frequency.

  public:
  FrequencyTrigger(int64_t lifetimeMs, unsigned long triggerIntervalMs); // constructor

  bool checkTrigger(); // Loop function

  // Lifecycle functions
  void activate(long delayMs = 0); // activates the trigger (after optional delay [milliseconds])
  void expire();                   // disables the trigger
  bool isExpired();                // returns true if the trigger is expired/disabled

  private:
  // behavioral parameters are lifetime-constants (provided at construction)
  const int64_t lifetimeMs;
  const int64_t triggerIntervalMs;

  // dynamic state parameters
  int64_t lastActivationObservedMilli;
  int64_t nextTriggerAtOrAfterMilli;
  bool expired;
};

class FrequencyToggler2 {

  // CLASS FrequencyToggler
  //
  // This class provides an on/off toggling that changes between the on- and off-state _once_ in
  // specified time intervals.
  //
  // The constructor instantiates a _disabled_ toggler, which can be enabled by calling `activate()`.
  // Once activated, the `checkToggle` function will return true on the first call within every time interval
  // of `toggleIntervalMs` milliseconds. If an inteval is missed (e.g., because the controller loop
  // is busy), the interval is skipped and the toggler will return true on the next interval as it would
  // otherwise.
  // The lifetime is measured from the point of latest activation. After the specified `lifetimeMs`
  // [milliseconds] has elapsed, or `expire()` is called, the toggler behaves as follows:
  //   * If the state is ON, when the lifetime expires or `expire()` is called, then `checkToggle()` switches
  //     the state to OFF on the subsequent call, returning true once.
  //   * If the state is OFF, when the lifetime expires or `expire()` is called, no state change occurs, i.e
  //     `checkToggle()` returns false.
  // All subsequent `checkToggle()` calls then return false, until `activate()` is called again.
  // toggler deactivates and no longer returns
  // true - until `activate()` is called again.
  // Negative lifetime means that the trigger remains active indefinitely until `expire()` is called.
  //
  // This implementation is intended to run on the controller loop, consuming minimal
  // resources. Results should be largely deterministic across different controllers as
  // we don't rely on CPU frequency.

  public:
  FrequencyToggler2(int64_t lifetimeMs, unsigned long toggleDurationOnMs, unsigned long toggleDurationOffMs); // constructor

  // checkToggle is intended to be called with high frequency, e.g. by the controller `loop`. It returns true _once_
  // the time for the next switch on <-> off has been reached or surpassed.
  // To find out whether the current state is on or off, the user must call `isCurrentStateOn()`.
  bool checkToggle();

  // Returns true if the current state is "on", false if "off".
  bool isCurrentStateOn();

  // Lifecycle functions
  void activate(long delayMs = 0); // activates the trigger (after optional delay [milliseconds])
  void expire();                   // disables the trigger
  bool isExpired();                // returns true if the trigger is expired/disabled (inverse of `isActive()`)
  bool isActive();                 // returns true if the trigger is active (irrespective whether the toggler's state is on or off)

  private:
  // Internally, the `checkToggle()` represents three states:
  //  * Active: the toggler is active and in the ON state
  //  * ShouldExpire: the toggler was active, and `expire()` was called - the next call to `checkToggle()` will switch it OFF,
  //  * Expired: the toggler is expired/disabled
  enum _status {
    Active = 0,
    ShouldExpire = 1,
    Expired = 2
  };

  // behavioral parameters are lifetime-constants (provided at construction)
  const int64_t lifetimeMs;
  const int64_t toggleDurationOnMs;
  const int64_t toggleDurationOffMs;

  // dynamic state parameters
  int64_t lastActivationObservedMilli;
  int64_t nextTriggerAtOrAfterMilli;
  bool stateIsOn;
  _status status;

  void advanceState(int64_t currentMillis);
};

class FrequencyToggler {

  // CLASS FrequencyToggler
  //
  // This class provides an on/off toggling that changes between the on- and off-state _once_ in
  // specified time intervals.
  //
  // The constructor instantiates a _disabled_ toggler, which can be enabled by calling `activate()`.
  // Once activated, the `checkToggle` function will return true on the first call within every time interval
  // of `toggleIntervalMs` milliseconds. If an inteval is missed (e.g., because the controller loop
  // is busy), the interval is skipped and the toggler will return true on the next interval as it would
  // otherwise.
  // The lifetime is measured from the point of latest activation. After the specified `lifetimeMs`
  // [milliseconds] has elapsed, or `expire()` is called, the toggler deactivates and no longer returns
  // true - until `activate()` is called again.
  // Negative lifetime means that the trigger remains active indefinitely until `expire()` is called.
  //
  // This implementation is intended to run on the controller loop, consuming minimal
  // resources. Results should be largely deterministic across different controllers as
  // we don't rely on CPU frequency.

  public:
  FrequencyToggler(int64_t lifetimeMs, unsigned long toggleIntervalMs); // constructor

  // checkToggle is intended to be called with high frequency, e.g. by the controller `loop`. It returns true _once_
  // the time for the next switch on <-> off has been reached or surpassed.
  // To find out whether the current state is on or off, the user must call `isCurrentStateOn()`.
  bool checkToggle();

  // Returns true if the current state is "on", false if "off".
  bool isCurrentStateOn();

  // Lifecycle functions
  void activate(long delayMs = 0); // activates the trigger (after optional delay [milliseconds])
  void expire();                   // disables the trigger
  bool isExpired();                // returns true if the trigger is expired/disabled (inverse of `isActive()`)
  bool isActive();                 // returns true if the trigger is active (irrespective whether the toggler's state is on or off)

  private:
  FrequencyToggler2 frequencyToggler2;
};
