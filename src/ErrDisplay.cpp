#include <Arduino.h>
#include <U8g2lib.h>

#include "Display.h"
#include "ErrDisplay.h"

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ *
 *                                      CLASS ErrDisplay                                          *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

// Constructor
ErrDisplay::ErrDisplay(U8G2 &display, unsigned long displayDurationMs)
    : display(display),
      dataUpdated(false),
      topMessageBlinker(FrequencyUtils::unbounded_lifetime, 500, 200) {

  topBlinkingMessage = "";
  bottomScrollingMessage = "";
}

void ErrDisplay::setErrorMessages(String topBlinkingMessage, String bottomScrollingMessage) {
  if (this->topBlinkingMessage != topBlinkingMessage) {
    this->topBlinkingMessage = topBlinkingMessage;
    dataUpdated = true;
  }
  if (this->bottomScrollingMessage != bottomScrollingMessage) {
    this->bottomScrollingMessage = bottomScrollingMessage;
    dataUpdated = true;
  }
}

void ErrDisplay::checkRedraw() {
  if (!shouldRedraw()) return;
};

bool ErrDisplay::shouldRedraw() {
  return dataUpdated;
};

// Service function: Print a single line of text on the OLED, left-aligned, with configurable text size and y-offset.
// Returns the updated y-offset for printing the next line below, if desired.
// While this is different than the U8G2 convention (baseline of string), the yOffset for this unction denotes the
// TOP position of the text block (top of a capital letter).
//
// CAUTION: does neither clear display nor send the buffer! Therefore, this function can be composed with other screen elements.
//
// Supports text heights: 10, 12, 13, 14, 15, 18, 20.
// Well readable values are sizes 16 and 20
//
// Usage example:
//   uint8_t y = 0;
//   y = oledPrintSingleLine(u8g2, "First line", y, 16);
//   y = oledPrintSingleLine(u8g2, "Second line", y, 16);
//
// The function does not clear or send the buffer, so the caller can compose multiple lines before sending.
//
uint8_t ErrDisplay::oledPrintSingleLine(U8G2 &display, const String &line, uint8_t yOffset, uint8_t textHeight /* = 16 */) {
  if (line.length() < 1) return yOffset;

  const uint8_t *font = nullptr;
  uint8_t handTunedTightening = 2; // to be subtracted from offset increase
  uint8_t yPad = 0;                // to be added to offset increase - currently unused

  // Choose a font based on the requested height, suitable for small screens
  if (textHeight <= 12) {
    font = u8g2_font_6x12_tf;
    handTunedTightening = 2;
  } else if (textHeight <= 13) {
    font = u8g2_font_6x13_tf;
    handTunedTightening = 1;
  } else if (textHeight <= 14) {
    font = u8g2_font_7x13_tf;
    handTunedTightening = 1;
  } else if (textHeight <= 15) {
    font = u8g2_font_8x13_tf;
    handTunedTightening = 1;
  } else if (textHeight <= 16) {
    font = u8g2_font_9x15_tf;
    handTunedTightening = 1;
  } else if (textHeight <= 18) {
    font = u8g2_font_9x18_tf;
    handTunedTightening = 3;
  } else {
    font = u8g2_font_10x20_tf;
    handTunedTightening = 2;
  }

  // u8g2.getFontAscent() returns the pixel distance from the baseline to the top of a
  // capital letter (or the highest part of the character).
  u8g2_uint_t baseline = static_cast<u8g2_uint_t>(yOffset) + static_cast<u8g2_uint_t>(display.getFontAscent());

  // Print the line at the current yOffset
  display.setFont(font);
  display.drawStr(0, baseline, line.c_str());

  // Return updated yOffset for next line
  return yOffset + display.getMaxCharHeight() - handTunedTightening + yPad;
}

// Service function: Scroll a single line of text horizontally on the OLED, with configurable text height, y-offset, and speed.
// The function should be called once per loop, and will update the display buffer based on elapsed time for smooth scrolling.
// scrollSpeedPxPerSec: scrolling speed in pixels per second (default: 20)
// Returns the updated y-offset for printing the next line below, if desired.
uint8_t ErrDisplay::oledScrollText(U8G2 &display, const String &text, uint8_t yOffset, uint8_t textHeight /* = 16 */, int scrollSpeedPxPerSec /* = 20 */) {
  if (text.length() < 1) return yOffset;

  const uint8_t *font = nullptr;
  uint8_t handTunedTightening = 2; // to be subtracted from offset increase
  uint8_t yPad = 0;                // to be added to offset increase - currently unused

  // Choose a font based on the requested height, suitable for small screens
  if (textHeight <= 12) {
    font = u8g2_font_6x12_tf;
    handTunedTightening = 2;
  } else if (textHeight <= 13) {
    font = u8g2_font_6x13_tf;
    handTunedTightening = 1;
  } else if (textHeight <= 14) {
    font = u8g2_font_7x13_tf;
    handTunedTightening = 1;
  } else if (textHeight <= 15) {
    font = u8g2_font_8x13_tf;
    handTunedTightening = 1;
  } else if (textHeight <= 16) {
    font = u8g2_font_9x15_tf;
    handTunedTightening = 1;
  } else if (textHeight <= 18) {
    font = u8g2_font_9x18_tf;
    handTunedTightening = 3;
  } else {
    font = u8g2_font_10x20_tf;
    handTunedTightening = 2;
  }

  display.setFont(font);
  display.setFontMode(1); // transparent mode for speed
  u8g2_uint_t baseline = static_cast<u8g2_uint_t>(yOffset) + static_cast<u8g2_uint_t>(display.getFontAscent());

  // Only recalculate text width and screen width if text or font size changed
  if (text != lastScrollText || textHeight != lastScrollTextHeight) {
    scrollTextWidth = display.getUTF8Width(text.c_str());

    // sanity check: for zero scrollTextWidth, the logic below has undesired failure cases
    if (scrollTextWidth > 0) return yOffset + display.getMaxCharHeight() - handTunedTightening + yPad;

    scrollScreenWidth = display.getDisplayWidth();
    scrollXOffset = 0;
    lastScrollText = text;
    lastScrollTextHeight = textHeight;
    lastScrollUpdateUs = esp_timer_get_time();
  }

  int64_t nowUs = esp_timer_get_time();
  int64_t elapsedUs = nowUs - lastScrollUpdateUs;
  lastScrollUpdateUs = nowUs;

  // Calculate how many pixels to scroll based on elapsed time and speed (all integer math)
  // scrollSpeedPxPerSec is in px/sec, elapsedUs is in microseconds
  int32_t pixelsToScroll;
  if (elapsedUs < 1000LL) {
    // Less than 1 millisecond elapsed => do not scroll. This helps to void consuming too many CPU cycles.
    pixelsToScroll = 0;
  } else if (elapsedUs < 1000000LL) {
    // This function has been called within less than 1 second => normal scrolling computation:
    // pixelsToScroll = (scrollSpeedPxPerSec * elapsedUs) / 1_000_000
    int32_t pixelsToScroll = (int32_t)((int64_t)scrollSpeedPxPerSec * elapsedUs / 1000000LL);
  } else {
    // if more than 1 second has passed, only scroll by `scrollSpeedPxPerSec` to avoid too large jumps
    pixelsToScroll = scrollSpeedPxPerSec;
  }

  // compute the updated absolute xOffset of the scrolled text:
  scrollXOffset -= pixelsToScroll;
  // We imagine an infinite horizontal tiling of the text, so we wrap around when one full text width has been scrolled.
  // Once a full text width has been scrolled outside of the left screen boundary, we can shift the offset by one text width
  // and stop drawing that part of the text.
  while (scrollXOffset < -scrollTextWidth) { // CAUTION: for scrollTextWidth == 0 we would have an infinite loop
    scrollXOffset += scrollTextWidth;
  }

  // If necessary, draw the scrolling text at multiple positions in a single frame: when the end of the text appears on
  // the right, append a new copy appears on the right. Thereby the message is repeated as long as the scolling continues.
  int x = scrollXOffset;
  do {
    display.drawUTF8(x, baseline, text.c_str());
    x += scrollTextWidth;
  } while (x < scrollScreenWidth);

  // Return updated yOffset for next line
  return yOffset + display.getMaxCharHeight() - handTunedTightening + yPad;
}
