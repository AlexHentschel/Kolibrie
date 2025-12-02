#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include <memory>

#include "Display.h"
#include "FrequentlyUtils.h"

class ErrDisplay {

  // CLASS ErrDisplay
  // encapsulates the u8g2 display logic for displaying some error status on the on-board 72x40 OLED screen
  // This display provides two lines. The top line is intended to show some short static error summary, which will
  // be displayed as blinking. The bottom line is intended to show some more detailed error description, which will
  // be scrolled if too long to fit on the display within the time of displayDurationMs.

  public:
  ErrDisplay(U8G2 &display, const DisplayText &topBlinkingMessage, const DisplayText &bottomScrollingMessage, u8g2_uint_t scrollSpeedPxPerSec = 15); // constructor

  // checkRedraw is intended to be called with high frequency, e.g. by the controller `loop`. It re-draws the
  // the display only if data has changed since the last draw.
  void checkRedraw();

  private:
  U8G2 &display_;

  // topMessageBlinker toggles the top error message on/off.
  const DisplayText topBlinkingMessage_;
  FrequencyToggler2 topMessageBlinker;

  const DisplayText bottomScrollingMessage_;
  DisplayScrollText bottomScroller;

  bool shouldRedraw(int64_t currentMicros);

  // State for scrolling text
  int scrollXOffset = 0;            // current scroll offset in pixels
  int scrollTextWidth = 0;          // width of the text in pixels
  int scrollScreenWidth = 0;        // width of the display in pixels
  int scrollSpeedPxPerSec = 20;     // scrolling speed in pixels per second (default 20)
  int64_t lastScrollUpdateUs = 0;   // last update time in microseconds
  String lastScrollText = "";       // last scrolled text
  uint8_t lastScrollTextHeight = 0; // last used text height

  // internal service methods
  // Print a single line of text at a given y-offset, return updated y-offset for next line
  uint8_t oledPrintSingleLine(U8G2 &display, const String &line, uint8_t yOffset, uint8_t textHeight = 16);
  uint8_t oledScrollText(U8G2 &display, const String &text, uint8_t yOffset, uint8_t textHeight = 16, int scrollSpeedPxPerSec = 20);
};