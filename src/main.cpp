#include <Arduino.h>
#include <U8g2lib.h>

// WIFI
#include <WiFi.h>
#include <WiFiClientSecure.h>

// custom utils
#include "LedUtils.h"

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ System CONFIGURATION ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */
// Wifi credentials:
#include "WiFiCredentials.h"

/* On-Board Screen (OLED 72x40)
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

U8G2_SSD1306_72X40_ER_F_SW_I2C u8g2(U8G2_R2, 6, 5, U8X8_PIN_NONE);
// U8G2_R0 	No rotation, landscape
// U8G2_R1 90 degree clockwise rotation
// U8G2_R2 180 degree clockwise rotation
// U8G2_R3 270 degree clockwise rotation

int width = 72;
int height = 40;
int xOffset = 28; // = (132-w)/2
int yOffset = 24; // = (64-h)/2

const char DEG_SYM[] = {0xB0, '\0'};

const unsigned int text1_y0 = 34, text2_y0 = 66;
const char *text1 = "Bunny Happyness ";             // scroll this text from right to left
const char *text2 = "The Cat Sleeps well tonight "; // scroll this text from right to left

/* LEDs
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
#define BLUE_LED_BUILTIN 8 // GPIO 8, Blue LED: LOW = on, HIGH = off

// LED Blinking patterns to indicate current state
LEDExpiringToggler *blueToggler = nullptr; // blinks 5 times turning o1 second

/* Controller Output for External Load -> GPIO
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
// EXT_LOAD_SWITCH defines the GPIO that is used to controll an external load attached.
// EXT_LOAD_ON and EXT_LOAD_OFF define the states that correspond to the load being provided
// power or not. Here, we use the Solid State Relay [SSR] H3MB-052D from Ingenex, which
// connects its load pins on input HIGH
#define EXT_LOAD_SWITCH 1 // GPIO 1 controls the external load (through a Mosfet supplying 5V trigger to SSR switching AC mains)
#define EXT_LOAD_ON HIGH
#define EXT_LOAD_OFF LOW

// For testing purposes, we are "misusing" an LED toggler to control the external load logic
LEDExpiringToggler *extLoadToggler = nullptr;

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ CONTROLLER INITIALIZATION ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */

/* FUNCTION PROTOTYPES
 * ╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴ */

/* FRAMEWORK FUNCTION setup(): called by Arduino framework once at startup
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

void setup() {
  Serial.begin(115200);
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setContrast(1);      // set contrast to maximum
  u8g2.setBusClock(400000); // 400kHz I2C

  u8g2.enableUTF8Print();
  u8g2.setFont(u8g2_font_logisoso30_tf); // set the target font to calculate the pixel width
  u8g2.setFontMode(0);                   // enable transparent mode, which is faster

  blueToggler = new LEDExpiringToggler(BLUE_LED_BUILTIN, 1350, 150, LEDExpiringToggler::LOW_IS_ON); // blinks 1 times turning o1 second
  blueToggler->trigger();
  while (true) {
    delay(20);
    blueToggler->toggleLED();
    if (blueToggler->isExpired()) break;
  }

  /* ── LEDs' blinking patterns to indicate current state ─────────── */
  blueToggler = new LEDExpiringToggler(BLUE_LED_BUILTIN, 999999999, 2000, LEDExpiringToggler::LOW_IS_ON); // blinks 1 times turning o1 second

  /* ── Toggling GPIO 1, which connects to Mosfet ─────────── */
  extLoadToggler = new LEDExpiringToggler(EXT_LOAD_SWITCH, 999999999, 2000, LEDExpiringToggler::HIGH_IS_ON); // blinks 1 times turning o1 second

  blueToggler->trigger();
  extLoadToggler->trigger();
  Serial.println("ESP32-C3 woken up!");
}

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ CONTROLLER LOOP ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */
/* Frequency bound for controller reconnection attempts in Milliseconds
 * ╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴ */

void loop() {
  u8g2.clearBuffer();                  // clear the internal memory
  u8g2.drawFrame(0, 0, width, height); // draw a frame around the border
  u8g2.setCursor(xOffset + 15, yOffset + 25);
  // u8g2.printf("%dx%d", width, height);
  u8g2.drawUTF8(0, text1_y0, "12"); // draw the scolling text

  u8g2.drawUTF8(42, text1_y0 + 6, "°"); // draw the scolling text
  u8g2.drawUTF8(54, text1_y0, "C");
  u8g2.sendBuffer(); // transfer internal memory to the display

  blueToggler->toggleLED();
  extLoadToggler->toggleLED();
}

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ BUSINESS LOGIC FUNCTIONS ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */

/* ...
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
