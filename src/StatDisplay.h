#pragma once
#include "FrequentlyUtils.h"
#include <Arduino.h>
#include <U8g2lib.h>

class StatDisplay {

  // CLASS StatDisplay
  //
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

  public:
  /**
   * Constructor
   * @param display Reference to the U8G2 display
   * @param heatingSymbolOnDurationMs Duration (ms, unsigned int) for which the blinking heating symbol is shown (converted to microseconds internally)
   * @param heatingSymbolOffDurationMs Duration (ms, unsigned int) for which the blinking heating symbol is OFF (converted to microseconds internally)
   */
  StatDisplay(U8G2 &display, unsigned int heatingSymbolOnDurationMs, unsigned int heatingSymbolOffDurationMs);

  /**
   * checkRedraw(int64_t currentMicros) is intended to be called with high frequency, e.g. by the controller `loop`.
   * It re-draws the display only if data has changed since the last draw or if a periodic redraw is needed.
   *
   * @param currentMicros Current time in microseconds (from esp_timer_get_time())
   */
  void checkRedraw(int64_t currentMicros);

  /**
   * checkRedraw() is a convenience function that calls esp_timer_get_time() internally to get the current time in microseconds.
   * This is less efficient, as esp_timer_get_time() is relatively expensive. For best performance, call esp_timer_get_time() once per controller loop
   * and pass the value to all instances via checkRedraw(int64_t currentMicros).
   */
  void checkRedraw();

  // Lifecycle functions
  void setTemp(float temp);             // sets the temperature to be displayed
  void setHeatingStatus(bool isOn);     // sets the heating status to be displayed
  void setWifiStatus(bool isConnected); // sets the wifi status to be displayed

  private:
  U8G2 &display;
  int temp;
  bool wifiConnected;

  // heatingStatusBlinker toggles the heating symbol on/off. We also use it directly to track whether
  // heating is on or off by using the FrequencyToggler2's `activate()` and `expire()` methods.
  FrequencyToggler2 heatingStatusBlinker;

  bool dataUpdated;

  /**
   * shouldRedraw(int64_t currentMicros) returns true if the display should be redrawn (data changed or periodic update needed).
   * @param currentMicros Current time in microseconds (from esp_timer_get_time())
   */
  bool shouldRedraw(int64_t currentMicros);
};
