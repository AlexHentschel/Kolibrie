#include <Arduino.h>
#include <U8g2lib.h>

#include "Display.h"
#include "ErrDisplay.h"

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ *
 *                                      CLASS ErrDisplay                                          *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

// Constructor
ErrDisplay::ErrDisplay(U8G2 &display, const DisplayText &topBlinkingMessage, const DisplayText &bottomScrollingMessage, u8g2_uint_t scrollSpeedPxPerSec /* = 15 */)
    : display_(display),
      topBlinkingMessage_(topBlinkingMessage),
      topMessageBlinker_(FrequencyUtils::unbounded_lifetime, 700, 300),
      bottomScrollingMessage_(bottomScrollingMessage),
      bottomScroller_(display_, bottomScrollingMessage_, topBlinkingMessage.getNextLineYOffset(), scrollSpeedPxPerSec) {
  topMessageBlinker_.activate();
  bottomScroller_.activate();
}

// Convenience: calls esp_timer_get_time() internally (less efficient)
void ErrDisplay::checkRedraw() {
  checkRedraw(esp_timer_get_time());
}

// Efficient: pass current time in microseconds (recommended for controller loop)
void ErrDisplay::checkRedraw(int64_t currentMicros) {
  if (!shouldRedraw(currentMicros)) return;

  display_.clearBuffer();
  if (topMessageBlinker_.isCurrentStateOn()) { // Top blinking Line
    Display::oledPrintSingleLine(display_, topBlinkingMessage_, 0);
  }
  bottomScroller_.draw(currentMicros); // Bottom Line
  display_.sendBuffer();
};

// not idempotent: consumes toggles, for internal use only
bool ErrDisplay::shouldRedraw(int64_t currentMicros) {
  if (topMessageBlinker_.checkToggle(currentMicros)) return true;
  if (bottomScroller_.shouldRedraw(currentMicros)) return true;
  return false;
};
