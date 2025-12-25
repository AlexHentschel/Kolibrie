#include <Arduino.h>
#include <U8g2lib.h>

#include "Display.h"
#include "StatDisplay.h"

namespace {
  constexpr int epd_bitmap_wifi_width = 12;
  constexpr int epd_bitmap_wifi_height = 12;

  // Toggler for blinking the "heating symbol" on the OLED screen when the external load is active
  // Char 'flash-8x.png' from the Open Iconic font https://github.com/iconic/open-iconic, down-scaled to 20x20 pixels
  constexpr int epd_bitmap_flash_width = 20;
  constexpr int epd_bitmap_flash_height = 20;
  const unsigned char epd_bitmap_flash[] PROGMEM = {
      0x00, 0x0E, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x07, 0x00, 0x80, 0x07, 0x00,
      0xC0, 0x03, 0x00, 0xC0, 0x7F, 0x00, 0xE0, 0x3F, 0x00, 0x40, 0x3E, 0x00,
      0x00, 0x1C, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x0E, 0x00,
      0x40, 0x2F, 0x00, 0xE0, 0x3F, 0x00, 0xC0, 0x1F, 0x00, 0xC0, 0x0F, 0x00,
      0xC0, 0x07, 0x00, 0x80, 0x03, 0x00, 0x80, 0x01, 0x00, 0x80, 0x00, 0x00};

  // Wifi symbol for display on OLED screen when wifi internet connection is active
  // Char `rss-8x.png' from the Open Iconic font https://github.com/iconic/open-iconic, down-scaled to 12x12 pixels
  const unsigned char epd_bitmap_wifi[] PROGMEM = {
      0x80, 0x07, 0xE0, 0x03, 0x30, 0x00, 0x18, 0x07, 0xCC, 0x03, 0x66, 0x00,
      0x32, 0x06, 0x93, 0x03, 0x9B, 0x00, 0xDB, 0x0E, 0x49, 0x0E, 0x00, 0x0E};
}

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ *
 *                                      CLASS StatDisplay                                         *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

// Encapsulates the u8g2 display logic for displaying the system status on the on-board 72x40 OLED screen.
//
// Internally, all time bookkeeping is done in microseconds for efficiency. All variables representing time
// have the suffix 'Micros'. Constructor takes milliseconds as input (unsigned int, with 'Ms' suffix) to reflect
// human-relevant time scales.
//
// There are two checkRedraw() functions:
//   1. checkRedraw(int64_t currentMicros): efficient, takes current time in microseconds (recommended for controller loop)
//   2. checkRedraw(): convenience, but less efficient (calls esp_timer_get_time() internally)
//      For best performance, call esp_timer_get_time() once per loop and pass the value to all instances.

// Constructor
StatDisplay::StatDisplay(U8G2 &display, unsigned int heatingSymbolOnDurationMs, unsigned int heatingSymbolOffDurationMs)
    : display(display),
      heatingStatusBlinker(FrequencyUtils::unbounded_lifetime, heatingSymbolOnDurationMs, heatingSymbolOffDurationMs),
      temp(0), wifiConnected(false), dataUpdated(false) {
}

void StatDisplay::setTemp(float temp) {
  if (!isfinite(temp)) {
    return; // Ignore invalid temperature values (NaN, Inf, -Inf)
  }
  int newTemp;
  if (temp <= -99.0f) {
    newTemp = -99;
  } else if (temp >= 99.0f) {
    newTemp = 99;
  } else {
    newTemp = static_cast<int>(std::round(temp));
  }

  if (newTemp != this->temp) {
    this->temp = newTemp;
    dataUpdated = true;
  }
}

void StatDisplay::setHeatingStatus(bool isOn) {
  if (heatingStatusBlinker.isActive() == isOn) return; // no state change
  dataUpdated = true;

  if (isOn) {
    heatingStatusBlinker.activate();
  } else {
    heatingStatusBlinker.expire();
  }
}

void StatDisplay::setWifiStatus(bool isConnected) {
  if (isConnected != this->wifiConnected) {
    this->wifiConnected = isConnected;
    dataUpdated = true;
  }
}

// Efficient: pass current time in microseconds (recommended for controller loop)
void StatDisplay::checkRedraw(int64_t currentMicros) {
  if (!shouldRedraw(currentMicros)) return;

  display.clearBuffer();                                              // clear the internal memory
  display.drawFrame(0, 0, Display::OLED_width, Display::OLED_height); // draw a frame around the border
  display.setBitmapMode(1);                                           // this helps with transparent drawing of bitmap backgrounds

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌----╌╌╌╌ Temperature ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  int t = this->temp;
  if (t >= 0) {
    display.setFont(u8g2_font_logisoso30_tf); // same font as for "°C" symbol, hence do not use reduced font
    display.setCursor(2, 34);
    display.print(t);
  } else {
    // for negative temperatures, use smaller font to accommodate minus sign
    display.setFont(u8g2_font_logisoso20_tn); // numbers-only font [ending "tn"]
    display.setCursor(0, 28);
    display.print('-');
    display.setCursor(11, 30);
    display.print(-t);
  }

  display.setFont(u8g2_font_logisoso30_tf); // need full font including special characters for '°' char
  display.drawUTF8(42, 40, "°");
  display.setFont(u8g2_font_logisoso18_tr); // font containing letters only [ending "tr"]
  display.drawUTF8(54, 22, "C");

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Blinking heating symbol ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  if (heatingStatusBlinker.isCurrentStateOn()) {
    display.drawXBMP(37, 15, epd_bitmap_flash_width, epd_bitmap_flash_height, epd_bitmap_flash);
  }

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Wifi symbol ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  if (this->wifiConnected) {
    display.drawXBMP(55, 25, epd_bitmap_wifi_width, epd_bitmap_wifi_height, epd_bitmap_wifi);
  }

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ lifecycle ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  display.sendBuffer();
  dataUpdated = false;
}

// Convenience: calls esp_timer_get_time() internally (less efficient)
void StatDisplay::checkRedraw() {
  checkRedraw(esp_timer_get_time());
}

bool StatDisplay::shouldRedraw(int64_t currentMicros) {
  if (dataUpdated) {
    return true;
  }
  if (heatingStatusBlinker.checkToggle(currentMicros)) {
    return true;
  }
  return false;
}
