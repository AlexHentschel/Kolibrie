#pragma once
#include "FrequentlyUtils.h"
#include <Arduino.h>
#include <U8g2lib.h>

class StatDisplay {

  // CLASS StatDisplay
  // encapsulates the u8g2 display logic for displaying the system status on the on-board 72x40 OLED screen

  public:
  StatDisplay(U8G2 &display, unsigned long heatingSymbolOnDurationMs, unsigned long heatingSymbolOffDurationMs); // constructor

  // checkRedraw is intended to be called with high frequency, e.g. by the controller `loop`. It re-draws the
  // the display only if data has changed since the last draw.
  void checkRedraw();

  // Lifecycle functions
  void setTemp(float temp);             // sets the temperature to be displayed
  void setHeatingStatus(bool isOn);     // sets the heating status to be displayed
  void setWifiStatus(bool isConnected); // sets the wifi status to be displayed

  private:
  U8G2 &display;
  int temp;
  bool wifiConnected;
  FrequencyToggler2 extLoadOnDisplayBlinker;

  bool dataUpdated;

  bool shouldRedraw();
};