#pragma once
#include <Arduino.h>

class PrintLifeSign {

  // This class prints a life-sign message to the Serial console at specified intervals.
  // It is intended to run on the controller loop, consuming minimal resources.

  public:
  PrintLifeSign(long lifetimeMs, unsigned long printIntervalMs, String message); // explicit on/off logic

  void checkConsolePrint();
  void trigger();
  void expire();
  bool isExpired();

  // behavioral parameters are lifetime-constants (provided at construction)
  const long lifetimeMs;
  const unsigned long printIntervalMs;
  const String message;

  // dynamic state parameters
  unsigned long lastTriggerObservedMilli;
  unsigned long nextPrintAtOrAfterMilli;
  bool expired;
};