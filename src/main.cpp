#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <U8g2lib.h>

#undef LED_BUILTIN
#define LED_BUILTIN 8

// OLED 72x40 screen is

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

u8g2_uint_t offset1, offset2; // current offset for the scrolling text
u8g2_uint_t width1, width2;   // pixel width of the scrolling text (must be lesser than 128 unless U8G2_16BIT is defined
const unsigned int text1_y0 = 34, text2_y0 = 66;
const char *text1 = "Radio Chaine 3 ";             // scroll this text from right to left
const char *text2 = "Katy Perry - Miss You More "; // scroll this text from right to left

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ CONTROLLER CONFIGURATION ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */

// WIFI
#include "WiFiCredentials.h"

#define LED_BUILTIN 8 // Blue; LOW = on, HIGH = off

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ CONTROLLER INITIALIZATION ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */

/* FUNCTION PROTOTYPES
 * ╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴ */

/* FRAMEWORK FUNCTION setup(): called by Arduino framework once at startup
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  delay(500);
  Serial.println("Hello ESP32-C3!!");

  // u8x8.begin();
  // u8x8.setPowerSave(0);
  // u8x8.setFont(u8x8_font_chroma48medium8_r);

  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setContrast(1);      // set contrast to maximum
  u8g2.setBusClock(400000); // 400kHz I2C

  // see https://github.com/olikraus/u8g2/issues/329

  // u8g2.setFont(u8g2_font_ncenB14_tr); // choose a suitable font
  // u8g2.setFont(u8g2_font_inb30_mr); // set the target font to calculate the pixel width
  u8g2.enableUTF8Print();
  u8g2.setFont(u8g2_font_logisoso30_tf); // set the target font to calculate the pixel width
  // u8g2.setFont(u8g2_font_ncenB10_tr);

  width1 = u8g2.getUTF8Width(text1); // calculate the pixel width of the text
  width2 = u8g2.getUTF8Width(text2); // calculate the pixel width of the text
  u8g2.setFontMode(0);               // enable transparent mode, which is faster
  offset1 = 0;                       // start over again
  offset2 = 0;                       // start over again
}

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ CONTROLLER LOOP ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */
/* Frequency bound for controller reconnection attempts in Milliseconds
 * ╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴ */

void loop() {
  digitalWrite(LED_BUILTIN, LOW);
  delay(50);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(2000);
  Serial.println("ESP32-C4 Loop!!");

  // u8g2.firstPage();
  // do {
  //   u8g2.setCursor(33, 29);
  //   u8g2.print("24-02-种");
  //   u8g2.setCursor(33, 46);
  //   u8g2.print("23:55:器");
  //   u8g2.drawFrame(30, 12, 72, 40);
  // } while (u8g2.nextPage());

  u8g2.clearBuffer();                  // clear the internal memory
  u8g2.drawFrame(0, 0, width, height); // draw a frame around the border
  u8g2.setCursor(xOffset + 15, yOffset + 25);
  // u8g2.printf("%dx%d", width, height);
  u8g2.drawUTF8(0, text1_y0, "21"); // draw the scolling text

  u8g2.drawUTF8(42, text1_y0 + 6, "°"); // draw the scolling text
  u8g2.drawUTF8(54, text1_y0, "C");

  // u8g2.drawUTF8(47, text1_y0, DEG_SYM); // prints degree symbol u8g2. drawStr (54, 17, "C");

  // u8g2.drawGlyph(47, text1_y0, 176);
  //  u8g2.print(" °C");

  u8g2.sendBuffer(); // transfer internal memory to the display

  // u8x8.drawString(0, 1, "Hello w.!");
  // u8x8.setInverseFont(1);
  // u8x8.drawString(0, 0, "012345678");
  // u8x8.setInverseFont(0);
  // u8x8.refreshDisplay();
  delay(2000);
}

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ BUSINESS LOGIC FUNCTIONS ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */

/* ...
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
