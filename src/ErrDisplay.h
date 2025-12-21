#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include <memory>

#include "Display.h"
#include "FrequentlyUtils.h"

class ErrDisplay {

  // CLASS ErrDisplay
  //
  // Encapsulates the u8g2 display logic for displaying some error status on the on-board 72x40 OLED screen.
  // This display provides two lines. The top line is intended to show some short static error summary, which will
  // be displayed as blinking. The bottom line is intended to show some more detailed error description, which will
  // be scrolled if too long to fit on the display.
  //
  // Internally, all time bookkeeping is done in microseconds for efficiency. All variables representing time
  // have the suffix 'Micros'.
  //
  // There are two checkRedraw() functions:
  //   1. checkRedraw(int64_t currentMicros): efficient, takes current time in microseconds (recommended for controller loop)
  //   2. checkRedraw(): convenience, but less efficient (calls esp_timer_get_time() internally)
  //      For best performance, call esp_timer_get_time() once per loop and pass the value to all instances.

  public:
  ErrDisplay(U8G2 &display, const DisplayText &topBlinkingMessage, const DisplayText &bottomScrollingMessage, u8g2_uint_t scrollSpeedPxPerSec = 15); // constructor

  // checkRedraw is intended to be called with high frequency, e.g. by the controller `loop`. It re-draws the
  // the display only if data has changed since the last draw.
  // Efficient: pass current time in microseconds (recommended for controller loop)
  void checkRedraw(int64_t currentMicros);

  // Convenience: calls esp_timer_get_time() internally (less efficient)
  void checkRedraw();

  private:
  U8G2 &display_;

  // topMessageBlinker toggles the top error message on/off.
  const DisplayText topBlinkingMessage_;
  FrequencyToggler2 topMessageBlinker_;

  const DisplayText bottomScrollingMessage_;
  DisplayScrollText bottomScroller_;

  bool shouldRedraw(int64_t currentMicros);
};
