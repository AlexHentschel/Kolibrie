#include "ConsoleUtils.h"

// CLASS PrintLifeSign
//
// This class prints a life-sign message to the Serial console at specified intervals.
// It is intended to run on the controller loop, consuming minimal resources.

// constructor:
PrintLifeSign::PrintLifeSign(long lifetimeMs, unsigned long printIntervalMs, String message)
    : lifetimeMs(lifetimeMs), printIntervalMs(printIntervalMs), lastTriggerObservedMilli(0), message(message) {
}

void PrintLifeSign::checkConsolePrint() {
  if (expired) return;
  unsigned long currentMillis = millis();
  unsigned long sinceTrigger = currentMillis - lastTriggerObservedMilli;

  // note: negative lifetimeMs indicates infinite lifetime
  if ((lifetimeMs >= 0) && (sinceTrigger > lifetimeMs)) {
    expired = true;
    return;
  }

  // still active within lifetime: check if we have reached or exceeded the next print time
  if (currentMillis >= nextPrintAtOrAfterMilli) {
    Serial.println(message);
    nextPrintAtOrAfterMilli += printIntervalMs; // schedule next print time
  }
}

void PrintLifeSign::trigger() {
  if (lifetimeMs == 0) return; // no lifetime, so we don't need to trigger
  lastTriggerObservedMilli = millis();
  expired = false;
  Serial.println(message);
  nextPrintAtOrAfterMilli = lastTriggerObservedMilli + printIntervalMs;
}

void PrintLifeSign::expire() { expired = true; }

bool PrintLifeSign::isExpired() { return expired; }
