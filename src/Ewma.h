#pragma once
#include <Arduino.h>

class Ewma {

  // CLASS Ewma
  //
  // Exponential weighted moving average [EWMA].
  // Intuitively, the smoothing factor `α` relates to the averaging time window. Let `α ≡ 1/N`, and consider that the
  // input changes from `v_old` to `v_new` as a step function. Then N is the number of samples required to move the
  // output average approximately 63% of the way from `v_old` to `v_new` (precisely: 1 - 1/e ≈ 63.2%).

  public:
  // Constructor: alpha should be in range [0, 1]. Out-of-range values are clamped.
  // Attention: 
  //  • alpha = 0 causes no updates (infinite smoothing window)
  //  • alpha = 1 causes no averaging, the "smoothed" value is set to the new value immediately
  // While those alpha values are not recommended in practice, they are allowed for completeness.
  Ewma(float alpha, float startValue = 0.0f);

  // Lifecycle functions
  float update(float nextValue); // adds a new sample, updates the average, and returns the new average
  float value();                 // returns the current average
  void reset(float value);       // resets the average to specified value

  private:
  const float alpha_;
  float currentAverage_;
};