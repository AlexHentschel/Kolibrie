#include "Ewma.h"

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ *
 *                     CLASS Ewma  -  Exponential weighted moving average                         *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

// Given a starting value S_0, and a series of input samples x_1, x_2, ..., x_t, the EWMA S_t is defined recursively as:
// S_i = α * x_i + (1 - α) * S_{i-1}
// Intuitively, the smoothing factor `α` relates to the averaging time window. Let `α ≡ 1/N`, and consider that the
// input changes from `v_old` to `v_new` as a step function. Then N is the number of samples required to move the
// output average approximately 63% of the way from `v_old` to `v_new` (precisely: 1 - 1/e ≈ 63.2%).

static float clampAlpha_(float alpha) {
  if (alpha < 0.0f) {
    Serial.println("Warning: alpha = 0 is not recommended in practice!");
    return 0.0f;
  }
  if (alpha > 1.0f) {
    Serial.println("Warning: alpha = 1 is not recommended in practice!");
    return 1.0f;
  }
  return alpha;
}

// Constructor: alpha should be in range [0, 1]. Out-of-range values are clamped.
// Attention:
//  • alpha = 0 causes no updates (infintie smoothing window)
//  • alpha = 1 causes no averaging, the "smoothed" value is set to the new value immediately
// While those alpha values are not recommended in practice, they are allowed for completeness.
Ewma::Ewma(float alpha, float startValue /*= 0.0f */)
    : alpha_(clampAlpha_(alpha)), // clamp alpha to valid range
      currentAverage_(startValue) {}

// adds a new sample, updates the average, and returns the new average
float Ewma::update(float nextValue) {
  // the following is equivalent to the standard EWMA update formula:
  // currentAverage_ = α * nextValue + (1 - α) * currentAverage_
  // However, rearranged to avoid an extra multiplication.
  currentAverage_ += alpha_ * (nextValue - currentAverage_);
  return currentAverage_;
}

// returns the current average
float Ewma::value() { return currentAverage_; }

// resets the average to specified value
void Ewma::reset(float value) { currentAverage_ = value; }