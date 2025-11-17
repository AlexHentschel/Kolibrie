#include "ConsoleUtils.h"
#include <cstdint>     // For int64_t
#include <esp_timer.h> // For esp_timer_get_time()

// CLASS PrintLifeSign
//
// This class prints a life-sign message to the Serial console at specified intervals.
// It is intended to run on the controller loop, consuming minimal resources.

// constructor:
PrintLifeSign::PrintLifeSign(int64_t lifetimeMs, unsigned long printIntervalMs, String message)
    : lifetimeMs(lifetimeMs), printIntervalMs(static_cast<int64_t>(printIntervalMs)), message(message) {
}

void PrintLifeSign::checkConsolePrint() {
  if (expired) return;
  int64_t currentMillis = esp_timer_get_time() / 1000LL; // convert microseconds returned by `esp_timer_get_time()` to milliseconds
  int64_t sinceActivation = currentMillis - lastActivationObservedMilli;

  // If the lifetime has expired, mark as expired and return (without printing).
  // note: negative lifetimeMs means no expiration
  if ((lifetimeMs >= 0LL) && (sinceActivation > lifetimeMs)) {
    expired = true;
    return;
  }

  // within lifetime, but still before next trigger time: nothing to do
  if (currentMillis < nextPrintAtOrAfterMilli) {
    return;
  }

  // if we have reached or exceeded the next trigger time, then print message and schedule next print
  Serial.println(message);
  nextPrintAtOrAfterMilli += printIntervalMs;        // schedule next print time
  while (currentMillis >= nextPrintAtOrAfterMilli) { // skip missed intervals
    nextPrintAtOrAfterMilli += printIntervalMs;
  }
}

void PrintLifeSign::activate(long delayMs /* = 0 */) {
  if (lifetimeMs == 0LL) return; // no lifetime, so we don't need to trigger
  lastActivationObservedMilli = esp_timer_get_time() / 1000LL + static_cast<int64_t>(delayMs);
  expired = false;

  // print on next call to `checkConsolePrint()` (after `delayMs` milliseconds)
  nextPrintAtOrAfterMilli = lastActivationObservedMilli;
}

void PrintLifeSign::expire() { expired = true; }

bool PrintLifeSign::isExpired() { return expired; }
