#include "StatDisplay.h"
#include <Arduino.h>
#include <U8g2lib.h>

#define __OLED_width 72
#define __OLED_height 40

#define __epd_bitmap_wifi_width 12
#define __epd_bitmap_wifi_height 12

// Toggler for blinking the "heating symbol" on the OLED screen when the external load is active
// Char 'flash-8x.png' from the Open Iconic font https://github.com/iconic/open-iconic, down-scaled to 20x20 pixels
#define __epd_bitmap_flash_width 20
#define __epd_bitmap_flash_height 20
const unsigned char _epd_bitmap_flash[] PROGMEM = {
    0x00, 0x0E, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x07, 0x00, 0x80, 0x07, 0x00,
    0xC0, 0x03, 0x00, 0xC0, 0x7F, 0x00, 0xE0, 0x3F, 0x00, 0x40, 0x3E, 0x00,
    0x00, 0x1C, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x0E, 0x00,
    0x40, 0x2F, 0x00, 0xE0, 0x3F, 0x00, 0xC0, 0x1F, 0x00, 0xC0, 0x0F, 0x00,
    0xC0, 0x07, 0x00, 0x80, 0x03, 0x00, 0x80, 0x01, 0x00, 0x80, 0x00, 0x00};

// Wify symbol for display on OLED screen when wifi internet connection is active
// Char `rss-8x.png' from the Open Iconic font https://github.com/iconic/open-iconic, down-scaled to 12x12 pixels
#define __epd_bitmap_wifi_width 12
#define __epd_bitmap_wifi_height 12
const unsigned char _epd_bitmap_wifi[] PROGMEM = {
    0x80, 0x07, 0xE0, 0x03, 0x30, 0x00, 0x18, 0x07, 0xCC, 0x03, 0x66, 0x00,
    0x32, 0x06, 0x93, 0x03, 0x9B, 0x00, 0xDB, 0x0E, 0x49, 0x0E, 0x00, 0x0E};

StatDisplay::StatDisplay(U8G2 &display, unsigned long heatingSymbolOnDurationMs, unsigned long heatingSymbolOffDurationMs)
    : display(display),
      extLoadOnDisplayBlinker(FrequencyUtils::unbounded_lifetime, heatingSymbolOnDurationMs, heatingSymbolOffDurationMs) {
  // Constructor
}

void StatDisplay::setTemp(float temp) {
  if (!isfinite(temp) || !isnan(temp)) {
    return; // Ignore invalid temperature values
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
  if (extLoadOnDisplayBlinker.isActive() == isOn) return; // no state change
  dataUpdated = true;

  if (isOn) {
    extLoadOnDisplayBlinker.activate();
  } else {
    extLoadOnDisplayBlinker.expire();
  }
}

void StatDisplay::setWifiStatus(bool isConnected) {
  if (isConnected != this->wifiConnected) {
    this->wifiConnected = isConnected;
    dataUpdated = true;
  }
}

void StatDisplay::checkRedraw() {
  display.clearBuffer();                                // clear the internal memory
  display.drawFrame(0, 0, __OLED_width, __OLED_height); // draw a frame around the border
  display.setBitmapMode(1);

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌----╌╌╌╌ Temperature ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  int t = this->temp;
  if (t >= 0) {
    display.setFont(u8g2_font_logisoso30_tf); // same font as for "°C" symbol, hence do not use reduced font
    // convert this->temp to string and draw
    // char tempStr[4]; // integer
    // snprintf(tempStr, sizeof(tempStr), "%d", this->temp);
    // display.drawUTF8(2, 34, tempStr);

    // display.setCursor(2, 34);
    // display.print(this->temp);
  } else {
    display.setFont(u8g2_font_logisoso26_tn); // numbers-only font [ending "tn"]
  }
  display.setCursor(2, 34);
  display.print(t);

  display.setFont(u8g2_font_logisoso30_tf);
  display.drawUTF8(42, 40, "°");
  display.setFont(u8g2_font_logisoso18_tf);
  display.drawUTF8(54, 22, "C");

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Blinking heating symbol ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  if ((extLoadOnDisplayBlinker.isActive()) && (extLoadOnDisplayBlinker.isCurrentStateOn())) {
    display.drawXBMP(37, 15, __epd_bitmap_flash_width, __epd_bitmap_flash_height, _epd_bitmap_flash);
  }

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Wifi symbol ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  if (this->wifiConnected) {
    display.drawXBMP(55, 25, __epd_bitmap_wifi_width, __epd_bitmap_wifi_height, _epd_bitmap_wifi);
  }

  dataUpdated = false;
  display.sendBuffer();
}

bool StatDisplay::shouldRedraw() {
  if (dataUpdated) return true;
  if (extLoadOnDisplayBlinker.checkToggle()) return true;

  return false;
};