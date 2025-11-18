#pragma once
#include <Arduino.h>

class Ewma {

  // CLASS Ewma
  //
  // Exponential weighted moving average [EWMA].
  // Intuitively, the smoothing factor `α` relates to the averaging time window. Let `α ≡ 1/N`, and consider that the
  // input changes from `v_old` to `v_new` as a step function. Then N is the number of samples required to move the
  // output average about 2/3 of the way from `v_old` to `v_new`.

  public:
  Ewma(float alpha, float startValue = 0.0f); // constructor

  // Lifecycle functions
  float update(float nextValue); // adds a new sample, updates the average, and returns the new average
  float value();                 // returns the current average
  void reset(float value);       // resets the average to specified value

  private:
  const float alpha;
  float currentAverage;
};