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
  // of `triggerIntervalMs` milliseconds. If an interval is missed (e.g., because the controller loop
  // is busy), the interval is skipped and the trigger will return true on the next interval as it would
  // otherwise.
  // The lifetime is measured from the point of latest activation. After the specified `lifetimeMs`
  // [milliseconds] has elapsed, or `expire()` is called, the trigger deactivates and no longer returns
  // true - until `activate()` is called again.
  // Negative lifetime means that the trigger remains active indefinitely until `expire()` is called.
  //
  // Internally, all time bookkeeping is done in microseconds for efficiency. All variables representing time
  // have the suffix 'Micros'. Constructors and activate() take milliseconds as input (unsigned int, with 'Ms'
  // suffix) to reflect human-relevant time scales. The different units are also reflected by types:
  //   • `unsigned int` represents durations and delays in Milliseconds. Those can only be positive and allow
  //      value in the range 0 to 2^32 -1 milliseconds on a 32-bit MCU (quite precisely 49 days and 17 hours).
  //   •  for lifetime (and internally for bookkeeping), we use `int64_t` , which allows representing both
  //      positive and negative times covering large operational time spans beyond 50 days.
  // to represent milliseconds, and `int64_t` to represent microseconds. Thereby, the user can specify delays
  // and durations of up to 49  (slightly exceeding) days (2^32 -1 milliseconds).
  //
  // There are two checkTrigger() functions:
  //   1. checkTrigger(int64_t currentMicros): efficient, takes current time in microseconds (recommended for controller loop)
  //   2. checkTrigger(): convenience, but less efficient (calls esp_timer_get_time() internally)
  //      For best performance, call esp_timer_get_time() once per loop and pass the value to all instances.

  public:
  // Constructor with both lifetime and trigger interval specified.
  // If `lifetimeMs` is negative (also FrequencyUtils::unbounded_lifetime), the trigger remains active
  // indefinitely until `expire()` is called.
  FrequencyTrigger(int64_t lifetimeMs, unsigned int triggerIntervalMs);
  // Constructor with only interval specified, uses unbounded lifetime
  FrequencyTrigger(unsigned int triggerIntervalMs)
      : FrequencyTrigger(FrequencyUtils::unbounded_lifetime, triggerIntervalMs) {}

  // checkTrigger is intended to be called with high frequency, e.g. by the controller `loop`. It returns true _once_
  // the time for the next trigger has been reached or surpassed.
  // For efficiency, the user should call `esp_timer_get_time()` once per loop and pass the value to all instances.
  // This is because `esp_timer_get_time()` is relatively expensive. Repetitive calls should be avoided, which happen
  // if `checkTrigger()` without parameters is used.

  bool checkTrigger(int64_t currentMicros); // Efficient: pass current time in microseconds
  bool checkTrigger();                      // Convenience: calls esp_timer_get_time() internally (less efficient)

  // Lifecycle functions
  void activate(unsigned int delayMs = 0); // activates the trigger (after optional delay [milliseconds])
  void expire();                           // disables the trigger
  bool isExpired();                        // returns true if the trigger is expired/disabled

  private:
  // behavioral parameters are lifetime-constants (provided at construction)
  const int64_t lifetimeMicros;
  const int64_t triggerIntervalMicros;

  // dynamic state parameters
  int64_t lastActivationObservedMicros;
  int64_t nextTriggerAtOrAfterMicros;
  bool expired;

  bool checkTrigger_(int64_t currentMicros); // Internal: does not check expiry

  void advanceState(int64_t currentMicros);
};

class CooldownTriggerN {

  // CLASS CooldownTriggerN
  //
  // This class provides a boolean trigger that triggers exactly N times. There are two mandatory parameters
  // that govern the behavior:
  //  * n [int]: the number of times to trigger.
  //    A negative `n` means that the trigger remains active indefinitely until `expire()` is called.
  //  * cooldownMs [unsigned int]: the minimum time interval [milliseconds] between subsequent triggers
  // In additional, the following optional parameter can be provided
  //  * delayMs [unsigned int] = 0 : an initial minimum delay [milliseconds] before the first trigger
  //   measured from the point of activation.
  //
  // During the cooldown or initial delay period, any calls to `checkTrigger()` return false. Thereafter,
  // `checkTrigger()` returns true once and the system enters the cooldown period again. This continues until
  // the trigger has fired exactly N times, after which all subsequent calls to `checkTrigger()` return false,
  // unless `activate()` is called again to reset the trigger.
  //
  // At any point, `expire()` may be called to disable the trigger. Any calls to `checkTrigger()` thereafter
  // return false unless `activate()` is called again to reset the trigger.
  //
  //
  // Internally, all time bookkeeping is done in microseconds for efficiency. All variables representing time
  // have the suffix 'Micros'. Constructors and `activate()` take milliseconds as input (unsigned int, with 'Ms'
  // suffix) to reflect human-relevant time scales. The different units are also reflected by types:
  //   • `unsigned int` represents durations and delays in Milliseconds. Those can only be positive and allow
  //      value in the range 0 to 2^32 -1 milliseconds on a 32-bit MCU (quite precisely 49 days and 17 hours).
  //   •  for internal bookkeeping, we use `int64_t`, which allows representing both
  //      positive and negative times covering large operational time spans beyond 50 days.
  //
  // There are two checkTrigger() functions:
  //   1. checkTrigger(int64_t currentMicros): efficient, takes current time in microseconds (recommended for controller loop)
  //   2. checkTrigger(): convenience, but less efficient (calls esp_timer_get_time() internally)
  //      For best performance, call esp_timer_get_time() once per loop and pass the value to all instances.

  public:
  // Constructor for a trigger that fires exactly `n` times, with minimal cooldown interval [milliseconds] between triggers.
  // After construction, the trigger is inactive/expired. Call `activate()` to start triggering.
  // A negative `n` means that the trigger remains active indefinitely until `expire()` is called.
  CooldownTriggerN(int n, unsigned int cooldownMs);

  // checkTrigger is intended to be called with high frequency, e.g. by the controller `loop`. It returns true _once_
  // the time for the next time has been reached or surpassed.
  // For efficiency, the user should call `esp_timer_get_time()` once per loop and pass the value to all instances.
  // This is because `esp_timer_get_time()` is relatively expensive. Repetitive calls should be avoided, which happen
  // if `checkTrigger()` without parameters is used.

  bool checkTrigger(int64_t currentMicros); // Efficient: pass current time in microseconds
  bool checkTrigger();                      // Convenience: calls esp_timer_get_time() internally (less efficient)

  // Lifecycle functions
  void activate(unsigned int delayMs = 0);                        // activates the trigger, fires immediately or after the optional delay [milliseconds]
  void activate(int64_t currentMicros, unsigned int delayMs = 0); // more efficient: activates using provided current time [microseconds]
  void expire();                                                  // disables the trigger
  bool isExpired();                                               // returns true if the trigger is expired/disabled

  private:
  // behavioral parameters are lifetime-constants (provided at construction)
  const int n;
  const int64_t cooldownMicros;

  // dynamic state parameters
  int remainingTriggers; // zero indicates expired/disabled; negative means infinite number of triggers remaining
  int64_t nextTriggerAtOrAfterMicros;

  void advanceState(int64_t currentMicros); // advances internal state after a trigger has fired
};

class FrequencyToggler2 {
  // CLASS FrequencyToggler2
  //
  // This class provides an on/off toggling that changes between the on- and off-state _once_ in
  // specified time intervals.
  //
  // The constructor instantiates a _disabled_ toggler, which can be enabled by calling `activate()`.
  // Once activated, the `checkToggle` function will return true on the first call within every time interval
  // of `toggleDurationOnMs` milliseconds. If an interval is missed (e.g., because the controller loop
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
  // Internally, all time bookkeeping is done in microseconds for efficiency. All variables representing time
  // have the suffix 'Micros'. Constructors and activate() take milliseconds as input (unsigned int, with 'Ms'
  // suffix) to reflect human-relevant time scales. To signify the difference, we use an `unsigned int` type
  // to represent milliseconds, and `int64_t` to represent microseconds. Thereby, the user can specify delays
  // and durations of up to 49 (slightly exceeding) days (2^32 -1 milliseconds) on a 32-bit MCU.
  //
  // There are two checkToggle() functions:
  //   1. checkToggle(int64_t currentMicros): efficient, takes current time in microseconds (recommended for controller loop)
  //   2. checkToggle(): convenience, but less efficient (calls esp_timer_get_time() internally)
  //      For best performance, call esp_timer_get_time() once per loop and pass the value to all instances.

  public:
  // Constructor with both lifetime and durations specified
  // If `lifetimeMs` is negative (also FrequencyUtils::unbounded_lifetime), the trigger remains active
  // indefinitely until `expire()` is called.
  FrequencyToggler2(int64_t lifetimeMs, unsigned int toggleDurationOnMs, unsigned int toggleDurationOffMs);
  // Constructor with only durations specified, uses unbounded lifetime
  FrequencyToggler2(unsigned int toggleDurationOnMs, unsigned int toggleDurationOffMs)
      : FrequencyToggler2(FrequencyUtils::unbounded_lifetime, toggleDurationOnMs, toggleDurationOffMs) {}

  // checkToggle is intended to be called with high frequency, e.g. by the controller `loop`. It returns true _once_
  // the time for the next switch on <-> off has been reached or surpassed. To find out whether the current state is
  // on or off, the user must call `isCurrentStateOn(optional time in microseconds)`.
  // For efficiency, the user should call `esp_timer_get_time()` once per loop and pass the value to all instances.
  // This is because `esp_timer_get_time()` is relatively expensive. Repetitive calls should be avoided, which happen
  // if `checkToggle()` without parameters is used.

  bool checkToggle(int64_t currentMicros); // Efficient: pass current time in microseconds
  bool checkToggle();                      // Convenience: calls esp_timer_get_time() internally (less efficient)

  // Returns true if the current state is "on", false if "off".
  bool isCurrentStateOn();

  // Lifecycle functions
  void activate(unsigned int delayMs = 0); // activates the trigger (after optional delay [milliseconds])
  void expire();                           // disables the trigger
  bool isExpired();                        // returns true if the trigger is expired/disabled (inverse of `isActive()`)
  bool isActive();                         // returns true if the trigger is active (irrespective whether the toggler's state is on or off)

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
  const int64_t lifetimeMicros;
  const int64_t toggleDurationOnMicros;
  const int64_t toggleDurationOffMicros;

  // dynamic state parameters
  int64_t lastActivationObservedMicros;
  int64_t nextTriggerAtOrAfterMicros;
  bool stateIsOn;
  _status status;

  bool checkToggle_(int64_t currentMicros); // Internal: does not check expiry
  void advanceState(int64_t currentMicros);
};

class FrequencyToggler {

  // CLASS FrequencyToggler
  //
  // This class provides an on/off toggling that changes between the on- and off-state _once_ in
  // specified time intervals.
  //
  // The constructor instantiates a _disabled_ toggler, which can be enabled by calling `activate()`.
  // Once activated, the `checkToggle` function will return true on the first call within every time interval
  // of `toggleIntervalMs` milliseconds. If an interval is missed (e.g., because the controller loop
  // is busy), the interval is skipped and the toggler will return true on the next interval as it would
  // otherwise.
  // The lifetime is measured from the point of latest activation. After the specified `lifetimeMs`
  // [milliseconds] has elapsed, or `expire()` is called, the toggler deactivates and no longer returns
  // true - until `activate()` is called again.
  // Negative lifetime means that the trigger remains active indefinitely until `expire()` is called.
  //
  // Internally, all time bookkeeping is done in microseconds for efficiency. All variables representing time
  // have the suffix 'Micros'. Constructors and activate() take milliseconds as input (unsigned int, with 'Ms'
  // suffix) to reflect human-relevant time scales. To signify the difference, we use an `unsigned int` type
  // to represent milliseconds, and `int64_t` to represent microseconds. Thereby, the user can specify delays
  // and durations of up to 49  (slightly exceeding) days (2^32 -1 milliseconds) on a 32-bit MCU.
  //
  // There are two checkToggle() functions:
  //   1. checkToggle(int64_t currentMicros): efficient, takes current time in microseconds (recommended for controller loop)
  //   2. checkToggle(): convenience, but less efficient (calls esp_timer_get_time() internally)
  //      For best performance, call esp_timer_get_time() once per loop and pass the value to all instances.

  public:
  // Constructor with both lifetime and interval specified.
  // If `lifetimeMs` is negative (also FrequencyUtils::unbounded_lifetime), the trigger remains active
  // indefinitely until `expire()` is called.
  FrequencyToggler(int64_t lifetimeMs, unsigned int toggleIntervalMs);
  // Constructor with only interval specified, uses unbounded lifetime
  FrequencyToggler(unsigned int toggleIntervalMs)
      : FrequencyToggler(FrequencyUtils::unbounded_lifetime, toggleIntervalMs) {}

  // checkToggle is intended to be called with high frequency, e.g. by the controller `loop`. It returns true _once_
  // the time for the next switch on <-> off has been reached or surpassed. To find out whether the current state is
  // on or off, the user must call `isCurrentStateOn(optional time in microseconds)`.
  // For efficiency, the user should call `esp_timer_get_time()` once per loop and pass the value to all instances.
  // This is because `esp_timer_get_time()` is relatively expensive. Repetitive calls should be avoided, which happen
  // if `checkToggle()` without parameters is used.

  bool checkToggle(int64_t currentMicros); // Efficient: pass current time in microseconds
  bool checkToggle();                      // Convenience: calls esp_timer_get_time() internally (less efficient)

  // Returns true if the current state is "on", false if "off".
  bool isCurrentStateOn();

  // Lifecycle functions
  void activate(unsigned int delayMs = 0); // activates the trigger (after optional delay [milliseconds])
  void expire();                           // disables the trigger
  bool isExpired();                        // returns true if the trigger is expired/disabled (inverse of `isActive()`)
  bool isActive();                         // returns true if the trigger is active (irrespective whether the toggler's state is on or off)

  private:
  FrequencyToggler2 frequencyToggler2_;
};
