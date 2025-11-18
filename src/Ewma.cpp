#include "Ewma.h"

// CLASS Ewma
//
// Exponential weighted moving average [EWMA].
// Intuitively, the smoothing factor `α` relates to the averaging time window. Let `α ≡ 1/N`, and consider that the
// input changes from `v_old` to `v_new` as a step function. Then N is the number of samples required to move the
// output average about 2/3 of the way from `v_old` to `v_new`.

// constructor:
Ewma::Ewma(float alpha, float startValue /*= 0.0f */) : alpha(alpha), currentAverage(startValue) {}

// adds a new sample, updates the average, and returns the new average
float Ewma::update(float nextValue) {
  // the following is equivalent to the standard EWMA update formula:
  // currentAverage = α * nextValue + (1 - α) * currentAverage
  // However, rearranged to avoid an extra multiplication.
  currentAverage += alpha * (nextValue - currentAverage);
  return currentAverage;
}

// returns the current average
float Ewma::value() { return currentAverage; }

// resets the average to specified value
void Ewma::reset(float value) { currentAverage = value; }