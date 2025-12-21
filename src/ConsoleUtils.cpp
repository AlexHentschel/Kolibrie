#include "ConsoleUtils.h"
#include <cstdint>     // For int64_t
#include <esp_timer.h> // For esp_timer_get_time()

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ *
 *                                     CLASS PrintLifeSign                                        *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

// This class prints a life-sign message to the Serial console at specified intervals.
// It is intended to run on the controller loop, consuming minimal resources.
// All internal time bookkeeping is done in microseconds for efficiency. All variables representing time
// have the suffix 'Micros'. Constructors and activate() take milliseconds as input (unsigned int, with 'Ms' suffix)
// to reflect human-relevant time scales. For best performance, call esp_timer_get_time() once per controller loop
// and pass the value to all instances. The function without time input is less efficient, as it calls esp_timer_get_time() internally.

// constructor:
PrintLifeSign::PrintLifeSign(int64_t lifetimeMs, unsigned int printIntervalMs, const String &message)
    : trigger_(lifetimeMs, printIntervalMs),
      message_(message) {}

// Efficient: pass current time in microseconds
void PrintLifeSign::checkConsolePrint(int64_t currentMicros) {
  if (trigger_.checkTrigger(currentMicros)) {
    Serial.println(message_);
  }
}

// Convenience: calls esp_timer_get_time() internally (less efficient)
void PrintLifeSign::checkConsolePrint() {
  // we let the internal trigger call `esp_timer_get_time()` instead of _always_ calling it here,
  // because `trigger.checkTrigger()` shortcuts the expensive `esp_timer_get_time()` call in various cases
  if (trigger_.checkTrigger()) {
    Serial.println(message_);
  }
}

void PrintLifeSign::activate(unsigned int delayMs /* = 0 */) {
  trigger_.activate(delayMs);
}

void PrintLifeSign::expire() {
  trigger_.expire();
}

bool PrintLifeSign::isExpired() {
  return trigger_.isExpired();
}
