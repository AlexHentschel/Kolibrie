#pragma once
#include <U8g2lib.h>

class DisplayText; // Forward declaration

namespace Display {
  constexpr u8g2_uint_t OLED_width = 72;
  constexpr u8g2_uint_t OLED_height = 40;

  // Service function: Print a single line of text on the OLED, left-aligned, with configurable text size and y-offset.
  // Returns the updated y-offset for printing the next line below, if desired.
  // While this is different than the U8G2 convention (baseline of string), the yOffset for this unction denotes the
  // TOP position of the text block (top of a capital letter).
  //
  // CAUTION: does neither clear display nor send the buffer! Therefore, this function can be composed with other screen elements.
  //
  // Supports text heights: 12, 13, 14, 15, 18, 20.
  // Well readable values are sizes 16 and 20
  //
  // Usage example:
  //   uint8_t y = 0;
  //   y = oledPrintSingleLine(u8g2, "First line", y, 16);
  //   y = oledPrintSingleLine(u8g2, "Second line", y, 16);
  //
  // The function does not clear or send the buffer, so the caller can compose multiple lines before sending.
  //
  u8g2_uint_t oledPrintSingleLine(U8G2 &display, const String &line, const u8g2_uint_t yOffset, const uint8_t textHeight = 16);
  u8g2_uint_t oledPrintSingleLine(U8G2 &display, const DisplayText &line, const u8g2_uint_t yOffset);
}

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ *
 *                                      CLASS DisplayText                                         *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

// DisplayText: Encapsulates font selection and text metrics for a single line of text.
class DisplayText {
  public:
  // Construct a DisplayText object for a given line and text height.
  // @param display: U8G2 display reference
  // @param line: text to display
  // @param yOffset: vertical offset (top of text block)
  // @param textHeight: font height (default 16)
  DisplayText(U8G2 &display, const String &line, uint8_t textHeight = 16);

  // Getters for font and metrics
  const uint8_t *font() const { return font_; }
  const String string() const { return text_; }
  u8g2_uint_t getFontAscent() const { return fontAscent_; }
  // Returns signed int intentionally: scrolling logic requires negative comparisons (e.g., scrollXOffset < -textWidth)
  int textWidth() const { return static_cast<int>(textWidth_); }
  unsigned int length() const;

  u8g2_uint_t getNextLineYOffset() const { return nextLineYOffset_; }
  const char *c_str() const { return text_.c_str(); }

  private:
  const String text_;
  const uint8_t *font_;
  u8g2_uint_t fontAscent_;
  u8g2_uint_t textWidth_;
  u8g2_uint_t nextLineYOffset_;
};

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ *
 *                                      CLASS DisplayScrollText                                   *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

// DisplayScrollText: Scroll a single line of text horizontally on the OLED.
// Call redraw() once per loop for smooth scrolling. Does not clear or send the buffer.
// All display and font parameters are precomputed for efficiency.
//
// Internally, all time bookkeeping is done in microseconds for efficiency. All variables representing time
// have the suffix 'Micros'. The activate() function takes milliseconds as input (unsigned int, with 'Ms'
// suffix) to reflect human-relevant time scales.
class DisplayScrollText {
  public:
  // Construct a scrolling text line for the OLED display.
  // @param display: U8G2 display reference
  // @param line: text to scroll
  // @param yOffset: vertical offset (top of text block)
  // @param textHeight: font height (default 16)
  // @param scrollSpeedPxPerSec: scroll speed in px/sec (default 15)
  DisplayScrollText(U8G2 &display, const String &line, u8g2_uint_t yOffset, uint8_t textHeight = 16, uint32_t scrollSpeedPxPerSec = 15);

  // Construct a scrolling text line for the OLED display.
  // @param display: U8G2 display reference
  // @param line: text to scroll
  // @param yOffset: vertical offset (top of text block)
  // @param scrollSpeedPxPerSec: scroll speed in px/sec (default 15)
  DisplayScrollText(U8G2 &display, const DisplayText &line, u8g2_uint_t yOffset, uint32_t scrollSpeedPxPerSec = 15);

  // Get the y-offset for the next line below this text.
  u8g2_uint_t getNextLineYOffset() const;

  // Destructor
  ~DisplayScrollText();

  // Lifecycle functions
  void activate(unsigned int delayMs = 0); // activates the scroller (after optional delay [milliseconds])
  void expire();                           // disables the scroller
  bool isExpired() const;                  // returns true if the scroller is expired/disabled (inverse of `isActive()`)
  bool isActive() const;                   // returns true if the scroller is active (irrespective whether the toggler's state is on or off)

  // Returns true if the display's content should be redrawn for this scrolling text. Call once per loop.
  // Efficient: pass current time in microseconds (recommended for controller loop)
  bool shouldRedraw(int64_t currentMicros);

  // Update the display buffer for this scrolling text. Always draws the current state.
  // Call once per loop after `shouldRedraw(currentMicros)` returned true. Or when other composed elements require a redraw.
  // Efficient: pass current time in microseconds (recommended for controller loop)
  void draw(int64_t currentMicros);

  private:
  // Internally, the `checkToggle()` represents three states:
  //  * Active: the toggler is active and in the ON state
  //  * ShouldExpire: the toggler was active, and `expire()` was called - the next call to `checkToggle()` will switch it OFF,
  //  * Expired: the toggler is expired/disabled
  enum _status {
    Active = 0,
    ShouldExpire = 1,
    Expired = 2
  };

  U8G2 &display_;
  DisplayText line_;

  // scrollSpeedPixelPerMibiUs_ denotes how many pixel we should ideally scroll per 1073.741824 seconds. This value is fixed for the lifetime of
  // the object and normalized such that we can avoid expensive integer division which are exceptionally expensive (especially 64-bit divisions)
  // on ESP32 microcontollers.The user provides `scrollSpeedPxPerSec` as input during construction, which denotes the number of pixels we scroll
  // per second. This is a unsigned 32-bit integer. We define:
  //   scrollSpeedPixel_ =  ⌊scrollSpeedPxPerSec · 2^30 / 10^6⌋ = ⌊ scrollSpeedPxPerSec · 1073.741824 ⌋
  // Here, ⌊○⌋ denotes the floor function. We note that the 64-bit integer scrollSpeedPixel_ cannot overflow, as `scrollSpeedPxPerSec`
  // is a 32-bit unsigned integer, which is shifted by 30 bits to the left, i.e. still fits into 64 bits (before division by 10^6). Therefore,
  // we only perform a single 64-bit division at construction time. During the runtime, the algorithm works with the `elapsed` microseconds since
  // the last scolling update. The number of pixels to scroll is:
  //   pixelsToScroll = ⌊ scrollSpeedPxPerSec · elapsed / 10^6 ⌋          the factor of 10^6 converts microseconds to seconds
  //                  = ⌊ scrollSpeedPixel_ · elapsed / 2^30 ⌋
  //                  = (scrollSpeedPixel_ · elapsed) >> 30               using bit shift to compute the division by 2^30
  const int64_t scrollSpeedPixel_;

  // Number of microseconds that need to elapse for one pixel scroll.
  // CAUTION: In C++, member variables are initialized in the order of their declaration in the class, NOT the order listed in the constructor's member initializer
  // list. Therefore, `scrollSpeedPixel_` must be declared before `onePixelDurationMicros_` in the class definition, because `onePixelDurationMicros_` depends on it.
  const int64_t onePixelDurationMicros_ = 0; // the duration in microseconds for one pixel scroll; formally inverse of scrollSpeedPxPerSec_

  const u8g2_uint_t yOffset_;

  _status status_;           // only text with Non-Zero width can reach state active = true
  int64_t startActiveMicros; // absolute "time", when we become active - caution in MICROseconds

  int64_t lastScrollUpdateMicros; // last time we updated the scroll position
  int scrollXOffset_;             // Tracks the current horizontal position for scrolling animation
};

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ *
 *                                      CLASS DisplayScrollText2                                  *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

// DisplayScrollText: Scroll a single line of text horizontally on the OLED.
// Call redraw() once per loop for smooth scrolling. Does not clear or send the buffer.
// All display and font parameters are precomputed for efficiency.
//
// Internally, all time bookkeeping is done in microseconds for efficiency. All variables representing time
// have the suffix 'Micros'. The activate() function takes milliseconds as input (unsigned int, with 'Ms'
// suffix) to reflect human-relevant time scales.
class DisplayScrollText2 {
  public:
  // Construct a scrolling text line for the OLED display.
  // @param display: U8G2 display reference
  // @param line: text to scroll
  // @param yOffset: vertical offset (top of text block)
  // @param textHeight: font height (default 16)
  // @param scrollSpeedPxPerSec: scroll speed in px/sec (default 15)
  DisplayScrollText2(U8G2 &display, const String &line, u8g2_uint_t yOffset, uint8_t textHeight = 16, u8g2_uint_t scrollSpeedPxPerSec = 15);

  // Construct a scrolling text line for the OLED display.
  // @param display: U8G2 display reference
  // @param line: text to scroll
  // @param yOffset: vertical offset (top of text block)
  // @param scrollSpeedPxPerSec: scroll speed in px/sec (default 15)
  DisplayScrollText2(U8G2 &display, const DisplayText &line, u8g2_uint_t yOffset, u8g2_uint_t scrollSpeedPxPerSec = 15);

  // Get the y-offset for the next line below this text.
  u8g2_uint_t getNextLineYOffset() const;

  // Destructor
  ~DisplayScrollText2();

  // Lifecycle functions
  void activate(unsigned int delayMs = 0); // activates the scroller (after optional delay [milliseconds])
  void expire();                           // disables the scroller
  bool isExpired() const;                  // returns true if the scroller is expired/disabled (inverse of `isActive()`)
  bool isActive() const;                   // returns true if the scroller is active (irrespective whether the toggler's state is on or off)

  // Returns true if the display's content should be redrawn for this scrolling text. Call once per loop.
  // Efficient: pass current time in microseconds (recommended for controller loop)
  bool shouldRedraw(int64_t currentMicros);

  // Update the display buffer for this scrolling text. Always draws the current state.
  // Call once per loop after `shouldRedraw(currentMicros)` returned true. Or when other composed elements require a redraw.
  // Efficient: pass current time in microseconds (recommended for controller loop)
  void draw(int64_t currentMicros);

  private:
  // Internally, the `checkToggle()` represents three states:
  //  * Active: the toggler is active and in the ON state
  //  * ShouldExpire: the toggler was active, and `expire()` was called - the next call to `checkToggle()` will switch it OFF,
  //  * Expired: the toggler is expired/disabled
  enum _status {
    Active = 0,
    ShouldExpire = 1,
    Expired = 2
  };

  U8G2 &display_;
  DisplayText line_;
  const u8g2_uint_t scrollSpeedPxPerSec_;
  const int64_t onePixelDurationMicros; // the duration in microseconds for one pixel scroll; formally inverse of scrollSpeedPxPerSec_
  const u8g2_uint_t yOffset_;

  _status status_;           // only text with Non-Zero width can reach state active = true
  int64_t startActiveMicros; // absolute "time", when we become active - caution in MICROseconds

  int64_t lastScrollUpdateMicros;
  int scrollXOffset_;
};
