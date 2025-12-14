#include <Arduino.h>
#include <Display.h>
#include <U8g2lib.h>

// Helper: select font and tuning for a given text height
// WRITES INTO `font` and `handTunedTightening`
static void selectFontForHeight(uint8_t textHeight, const uint8_t *&font, u8g2_uint_t &handTunedTightening, u8g2_uint_t &yPad) {
  yPad = 0;
  if (textHeight <= 12) {
    font = u8g2_font_6x12_tr;
    handTunedTightening = 2;
  } else if (textHeight <= 13) {
    font = u8g2_font_6x13_tr;
    handTunedTightening = 1;
  } else if (textHeight <= 14) {
    font = u8g2_font_7x13_tr;
    handTunedTightening = 1;
  } else if (textHeight <= 15) {
    font = u8g2_font_8x13_tr;
    handTunedTightening = 1;
  } else if (textHeight <= 16) {
    font = u8g2_font_9x15_tr;
    handTunedTightening = 1;
  } else if (textHeight <= 18) {
    font = u8g2_font_9x18_tr;
    handTunedTightening = 3;
  } else {
    font = u8g2_font_10x20_tr;
    handTunedTightening = 2;
  }
}

// Service function: Print a single line of text on the OLED, left-aligned, with configurable text size and y-offset.
// Returns the updated y-offset for printing the next line below, if desired.
// While this is different than the U8G2 convention (baseline of string), the yOffset for this unction denotes the
// TOP position of the text block (top of a capital letter).
//
// CAUTION: does neither clear display nor send the buffer! Therefore, this function can be composed with other screen elements.
//
// Supports text heights: 12, 13, 14, 15, 16, 18, 20.
// Well readable values are sizes 16 and 20
//
// Usage example:
//   uint8_t y = 0;
//   y = oledPrintSingleLine(u8g2, "First line", y, 16);
//   y = oledPrintSingleLine(u8g2, "Second line", y, 16);
//
// The function does not clear or send the buffer, so the caller can compose multiple lines before sending.
//
u8g2_uint_t Display::oledPrintSingleLine(U8G2 &display, const String &line, const u8g2_uint_t yOffset, const uint8_t textHeight) {
  if (line.length() < 1) return yOffset;

  const uint8_t *font_;
  u8g2_uint_t handTunedTightening_;
  u8g2_uint_t yPad_; // currently unused
  selectFontForHeight(textHeight, font_, handTunedTightening_, yPad_);

  // u8g2.getFontAscent() returns the pixel distance from the baseline to the top of a
  // capital letter (or the highest part of the character).
  u8g2_uint_t baseline = yOffset + static_cast<u8g2_uint_t>(display.getFontAscent());

  // Print the line at the current yOffset
  display.setFont(font_);
  display.drawStr(0, baseline, line.c_str());

  // Return updated yOffset for next line
  return yOffset + display.getMaxCharHeight() - handTunedTightening_ + yPad_;
}

u8g2_uint_t Display::oledPrintSingleLine(U8G2 &display, const DisplayText &line, const u8g2_uint_t yOffset) {
  if (line.length() < 1) return yOffset;

  // u8g2.getFontAscent() returns the pixel distance from the baseline to the top of a
  // capital letter (or the highest part of the character).
  u8g2_uint_t baseline = yOffset + line.getFontAscent();

  // Print the line at the current yOffset
  display.setFont(line.font());
  display.drawStr(0, baseline, line.c_str());

  // Return updated yOffset for next line
  return yOffset + line.getNextLineYOffset();
}

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ *
 *                                       CLASS DisplayText                                        *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

DisplayText::DisplayText(U8G2 &display, const String &text, uint8_t textHeight) : text_(text) {
  u8g2_uint_t handTunedTightening_;
  u8g2_uint_t yPad_; // currently unused

  selectFontForHeight(textHeight, font_, handTunedTightening_, yPad_);
  display.setFont(font_);
  fontAscent_ = static_cast<u8g2_uint_t>(display.getFontAscent());
  textWidth_ = display.getUTF8Width(text_.c_str());
  nextLineYOffset_ = display.getMaxCharHeight() - handTunedTightening_ + yPad_;
}

unsigned int DisplayText::length() const { return text_.length(); }

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ *
 *                                   CLASS DisplayScrollText                                      *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

// bound: enforce scroll speed limits in constructor
static u8g2_uint_t clampScrollSpeed2(u8g2_uint_t scrollSpeedPxPerSec) {
  if (scrollSpeedPxPerSec < 1) return 1;
  if (scrollSpeedPxPerSec > Display::OLED_width) return Display::OLED_width;
  return scrollSpeedPxPerSec;
}

static int64_t computeOnePixelDurationUs2_(u8g2_uint_t scrollSpeedPxPerSec) {
  u8g2_uint_t s = clampScrollSpeed2(scrollSpeedPxPerSec);
  return 1000000LL / static_cast<int64_t>(s);
}

DisplayScrollText::DisplayScrollText(U8G2 &display, const String &line, u8g2_uint_t yOffset, uint8_t textHeight, u8g2_uint_t scrollSpeedPxPerSec /* = 15 */)
    : display_(display),
      line_(display, line, textHeight),
      yOffset_(yOffset),
      scrollSpeedPxPerSec_(clampScrollSpeed2(scrollSpeedPxPerSec)),
      onePixelDurationMicros(computeOnePixelDurationUs2_(scrollSpeedPxPerSec_)),
      status_(_status::Expired),
      scrollXOffset_(0),
      lastScrollUpdateMicros(0) {
  //
}

DisplayScrollText::DisplayScrollText(U8G2 &display, const DisplayText &line, u8g2_uint_t yOffset, u8g2_uint_t scrollSpeedPxPerSec /* = 15 */)
    : display_(display),
      line_(line),
      yOffset_(yOffset),
      scrollSpeedPxPerSec_(clampScrollSpeed2(scrollSpeedPxPerSec)),
      onePixelDurationMicros(computeOnePixelDurationUs2_(scrollSpeedPxPerSec_)),
      status_(_status::Expired),
      scrollXOffset_(0),
      lastScrollUpdateMicros(0) {
  //
}

DisplayScrollText::~DisplayScrollText() {
  // No dynamic memory to free, but method provided for completeness
}

void DisplayScrollText::activate(unsigned int delayMs /* = 0 */) {
  if (line_.textWidth() < 1) return;

  status_ = _status::Active;
  startActiveMicros = esp_timer_get_time() + static_cast<int64_t>(delayMs) * 1000LL;
  lastScrollUpdateMicros = startActiveMicros;
  scrollXOffset_ = 0;
}

void DisplayScrollText::expire() {
  if (status_ != _status::Expired)
    status_ = _status::ShouldExpire;
}

bool DisplayScrollText::isExpired() const { return !isActive(); }

bool DisplayScrollText::isActive() const { return status_ == _status::Active; }

u8g2_uint_t DisplayScrollText::getNextLineYOffset() const {
  return yOffset_ + line_.getNextLineYOffset();
}

// draw always draws the current state of the scrolling text.
// Internal function intended to be composed with checks whether a redraw is necessary.
void DisplayScrollText::draw(int64_t currentMicros) {
  if (status_ > _status::Active) {
    if (status_ == _status::ShouldExpire) {
      status_ = _status::Expired;
    }
    return;
  }
  if (currentMicros < startActiveMicros) return; // not yet active

  int64_t elapsed = currentMicros - lastScrollUpdateMicros;
  int32_t pixelsToScroll = static_cast<int32_t>(elapsed / onePixelDurationMicros);
  if (1 <= pixelsToScroll) { // we only update the time reference if we actually scroll at least one pixel
    lastScrollUpdateMicros = currentMicros;
  }
  if (pixelsToScroll > scrollSpeedPxPerSec_) { // more than 1 second elapsed
    pixelsToScroll = scrollSpeedPxPerSec_;     // limit to maximally one second worth of scrolling
  }

  scrollXOffset_ -= pixelsToScroll;
  while (scrollXOffset_ < -line_.textWidth()) {
    scrollXOffset_ += line_.textWidth();
  }

  int x = scrollXOffset_;
  display_.setFont(line_.font());
  display_.setBitmapMode(1); // helps with smoother scrolling (?)
  do {
    display_.drawUTF8(x, yOffset_ + line_.getFontAscent(), line_.c_str());
    x += line_.textWidth();
  } while (x < Display::OLED_width);
}

// Efficient: pass current time in microseconds (recommended for controller loop)
bool DisplayScrollText::shouldRedraw(int64_t currentMicros) {
  if (status_ == _status::Expired) return false;       // expired
  if (currentMicros < startActiveMicros) return false; // not yet active

  int64_t elapsed = currentMicros - lastScrollUpdateMicros;
  return (elapsed >= onePixelDurationMicros);
};
