#include <Arduino.h>
#include <U8g2lib.h>

// WIFI
#include <WiFi.h>
#include <WiFiClientSecure.h>

// DS18B20 Temperature Sensor Libraries
#include "DallasTemperature.h"
#include "OneWire.h"

// Custom utils
#include "ConsoleUtils.h"
#include "FrequentlyUtils.h"
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

/* DS18B20 Temperature Sensor
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
#define TEMPERATURE_SENSOR_GPIO 2 // DS18B20 is connected to GPIO 2; this is the port for the OneWire bus
#define TEMPERATURE_PRECISION 10  // select 10 bit precision for DS18B20 (available range is 9 to 12 bits): corresponds to 0.25°C resolution with 187.5 ms measurement duration
OneWire temperatureSensorBus(TEMPERATURE_SENSOR_GPIO);
DallasTemperature temperatureSensors(&temperatureSensorBus);

DeviceAddress tempSensorDeviceAddress; // type definition for DS18B20 address (8 bytes), provided by DallasTemperature library
FrequencyTrigger *readTriggerTemperature = nullptr;

/* LEDs
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
#define BLUE_LED_BUILTIN 8 // GPIO 8, Blue LED: LOW = on, HIGH = off

// LED Blinking patterns to indicate current state
LEDExpiringToggler *blueToggler = nullptr; // blinks 5 times turning o1 second

/* Controller for External Load -> GPIO
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
// EXT_LOAD_SWITCH defines the GPIO that is used to controll an external load attached.
// EXT_LOAD_ON and EXT_LOAD_OFF define the states that correspond to the load being provided
// power or not. Here, the micro-controller's GPIO (3.3V) controls the external load, but
// through an IRL530 Power Mosfet, supplying 5V trigger to a Solid-State-Relay switching AC mains.
#define EXT_LOAD_SWITCH 1 // GPIO 1 controls the external load (through a Mosfet supplying 5V trigger to SSR switching AC mains)
#define EXT_LOAD_ON HIGH
#define EXT_LOAD_OFF LOW

// For testing purposes, we are "misusing" an LED toggler to control the external load logic
LEDExpiringToggler *extLoadToggler = nullptr;

// Toggler for blinking the "heating symbol" on the OLED screen when the external load is active
// Char 'flash-8x.png' from the Open Iconic font https://github.com/iconic/open-iconic, down-scaled to 20x20 pixels
#define epd_bitmap_flash_width 20
#define epd_bitmap_flash_height 20
const unsigned char epd_bitmap_flash[] PROGMEM = {
    0x00, 0x0E, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x07, 0x00, 0x80, 0x07, 0x00,
    0xC0, 0x03, 0x00, 0xC0, 0x7F, 0x00, 0xE0, 0x3F, 0x00, 0x40, 0x3E, 0x00,
    0x00, 0x1C, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x0E, 0x00,
    0x40, 0x2F, 0x00, 0xE0, 0x3F, 0x00, 0xC0, 0x1F, 0x00, 0xC0, 0x0F, 0x00,
    0xC0, 0x07, 0x00, 0x80, 0x03, 0x00, 0x80, 0x01, 0x00, 0x80, 0x00, 0x00};

// Wify symbol for display on OLED screen when wifi internet connection is active
// Char `rss-8x.png' from the Open Iconic font https://github.com/iconic/open-iconic, down-scaled to 12x12 pixels
#define epd_bitmap_wifi_width 12
#define epd_bitmap_wifi_height 12
const unsigned char epd_bitmap_wifi[] PROGMEM = {
    0x80, 0x07, 0xE0, 0x03, 0x30, 0x00, 0x18, 0x07, 0xCC, 0x03, 0x66, 0x00,
    0x32, 0x06, 0x93, 0x03, 0x9B, 0x00, 0xDB, 0x0E, 0x49, 0x0E, 0x00, 0x0E};

// Array of all bitmaps for convenience. (Total bytes used to store images in PROGMEM = 96)
const int epd_bitmap_allArray_LEN = 1;
const unsigned char *epd_bitmap_allArray[1] = {epd_bitmap_flash};

FrequencyToggler *extLoadOnDisplayBlinker = nullptr;

/* Life-Signs
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
// prints life-signs to Serial console, unbounded runtime, print every 5000 milliseconds
PrintLifeSign *consolePrintLifeSign = new PrintLifeSign(-1, 5000, "Controller alive");

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ CONTROLLER INITIALIZATION ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */

/* FUNCTION PROTOTYPES
 * ╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴ */
uint8_t scanDevicesAddressesAndRememberLast(OneWire &bus, DeviceAddress addressOut);
void printDeviceAddress(const DeviceAddress address);
void printTemperature(DallasTemperature &sensors, DeviceAddress deviceAddress);

void oledPrintTwoLines(U8G2 &display, const char *line1, const char *line2, uint8_t textHeight = 16);
void oledScrollText(U8G2 &display, const char *text, uint8_t textHeight = 16, uint16_t scrollSpeedMs = 50);

/* FRAMEWORK FUNCTION setup(): called by Arduino framework once at startup
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

void setup() { /* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
  Serial.begin(115200);
  delay(1000);

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ On-Board Screen (OLED 72x40) ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setContrast(1);      // set contrast to maximum
  u8g2.setBusClock(400000); // 400kHz I2C

  u8g2.enableUTF8Print();
  u8g2.setFont(u8g2_font_logisoso30_tf); // set the target font to calculate the pixel width
  u8g2.setFontMode(0);                   // enable transparent mode, which is faster

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Temperature Sensor ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  Serial.print(F("Scanning for OneWire devices on GPIO pin "));
  Serial.println(TEMPERATURE_SENSOR_GPIO, DEC);

  uint8_t deviceCount = scanDevicesAddressesAndRememberLast(temperatureSensorBus, tempSensorDeviceAddress); // scan for connected DS18B20 devices
  if (deviceCount != 1) {
    while (true) {
      Serial.print(F("Error: Expected exactly 1 DS18B20 device, but found "));
      Serial.print(deviceCount, DEC);
      Serial.println(F(" devices. Halting execution."));
      delay(5000);
    }
  }
  Serial.print(F("Assuming last detected device with address "));
  printDeviceAddress(tempSensorDeviceAddress);
  Serial.println(F(" to be the expected DS18B20 temperature sensor\n"));

  temperatureSensors.begin(); // Initialise the sensor.

  // Check that sensor is not reporting parasite power mode, which would not be expected and likely a symptom of some defect
  if (temperatureSensors.readPowerSupply(tempSensorDeviceAddress)) { // Read device's power requirements. Return 1 if device needs parasite power.
    Serial.print(F("WARNING: DS18B20 temperature sensor "));
    printDeviceAddress(tempSensorDeviceAddress);
    Serial.println(F(" is reporting PARASITE POWER MODE. This is unexpected and may indicate a defect."));
  }

  // set the temperature accuracy
  // Note on `skipGlobalBitResolutionCalculation` parameter:
  // When skipGlobalBitResolutionCalculation is set to true, the function will only set the resolution for the targeted device and will not recalculate or update the overall (global) bit
  // resolution for all devices on the bus. This can be useful for performance reasons or when you want to manage device resolutions individually without affecting the global setting.
  // Conversely, if skipGlobalBitResolutionCalculation is false, the function will update the global bit resolution variable after successfully setting the device's resolution. It will als
  // scan all devices to ensure the global bit resolution reflects the highest resolution among all connected sensors. This ensures consistency when reading temperatures from multiple devices.
  temperatureSensors.setResolution(tempSensorDeviceAddress, TEMPERATURE_PRECISION);

  // verify resolution setting:
  uint8_t actualPrecision = temperatureSensors.getResolution(tempSensorDeviceAddress);
  if (actualPrecision != TEMPERATURE_PRECISION) {
    Serial.print(F("Error: Unable to set DS18B20 temperature sensor "));
    printDeviceAddress(tempSensorDeviceAddress);
    Serial.print(F(" to desired precision of "));
    Serial.print(TEMPERATURE_PRECISION, DEC);
    Serial.println(F(" bits."));
    Serial.println(F("Sensor reports precision of "));
    Serial.print(actualPrecision, DEC);
    Serial.println(F(" bits."));
  }

  readTriggerTemperature = new FrequencyTrigger(-1, 5000); // read temperature every 2s, unbounded lifetime
  extLoadOnDisplayBlinker = new FrequencyToggler(-1, 500); // blinks every 500ms when activated

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ LEDs ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  // blinks quickly every 300ms for a total duration of 1.35s to indicate system is starting up
  blueToggler = new LEDExpiringToggler(BLUE_LED_BUILTIN, 1350, 150, LedUtils::LOW_IS_ON);
  blueToggler->activate();
  while (true) {
    delay(20);
    blueToggler->checkToggleLED();
    if (blueToggler->isExpired()) break;
  }

  /* ── LEDs' blinking patterns to indicate current state ─────────── */
  blueToggler = new LEDExpiringToggler(BLUE_LED_BUILTIN, -1, 2000, LedUtils::LOW_IS_ON); // blinks 1 times turning o1 second

  /* ── Toggling GPIO 1, which connects to Mosfet ─────────── */
  extLoadToggler = new LEDExpiringToggler(EXT_LOAD_SWITCH, -1, 2000, LedUtils::HIGH_IS_ON); // blinks 1 times turning o1 second

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ start ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  blueToggler->activate();
  extLoadToggler->activate();

  consolePrintLifeSign->activate(293);
  readTriggerTemperature->activate(421);
  extLoadOnDisplayBlinker->activate(421);
  Serial.println(F("Done with setup. Kolibrie commencing operations!"));

  // oledScrollText(u8g2, "Done with setup. Kolibrie commencing operations!", 20, 10);
  // delay(5000);
}

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ CONTROLLER LOOP ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */
/*
 * ╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴ */

void loop() { /* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

  u8g2.setFont(u8g2_font_logisoso30_tf);
  u8g2.clearBuffer();                  // clear the internal memory
  u8g2.drawFrame(0, 0, width, height); // draw a frame around the border
  // u8g2.setCursor(xOffset + 15, yOffset + 25);
  // u8g2.printf("%dx%d", width, height);
  u8g2.drawUTF8(2, 34, "82"); // draw the scolling text

  u8g2.drawUTF8(42, 40, "°"); // draw the scolling text
  u8g2.setFont(u8g2_font_logisoso18_tf);
  u8g2.drawUTF8(54, 22, "C");
  u8g2.sendBuffer(); // transfer internal memory to the display

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Temperature Sensor ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  temperatureSensors.requestTemperatures();

  if (readTriggerTemperature->checkTrigger()) {
    Serial.print("Celsius temperature: ");
    // We need to provide a "sensor index", as there can be more than one IC on the same bus. 0 refers to the first IC on the wire
    Serial.print(temperatureSensors.getTempCByIndex(0));
    Serial.print(" - Fahrenheit temperature: ");
    Serial.println(temperatureSensors.getTempFByIndex(0));
  }

  Serial.print("Toggler state: ");
  Serial.println(extLoadOnDisplayBlinker->isCurrentStateOn());

  if (extLoadOnDisplayBlinker->checkToggle()) {
    // toggle the heating symbol on the OLED display
    if (extLoadOnDisplayBlinker->isCurrentStateOn()) {
      Serial.println(" toggle heating symbol ON display");
      // u8g2.setFont(u8g2_font_open_iconic_embedded_2x_t);
      // u8g2.drawUTF8(38, 35, "\x43"); // draw heating symbol
      u8g2.setBitmapMode(1);
      // u8g2.drawXBMP(37, 15, epd_bitmap_flash_width, epd_bitmap_flash_height, epd_bitmap_flash);
      u8g2.drawXBMP(55, 25, epd_bitmap_wifi_width, epd_bitmap_wifi_height, epd_bitmap_wifi);

      // alternative for wifi symbol:
      // u8g2.setFont(u8g2_font_open_iconic_embedded_2x_t);
      // u8g2.drawUTF8(54, 45, "\x50");
    } else {
      // Serial.println(" toggle heating symbol OFF display");
      // // clear heating symbol area
      // u8g2.setDrawColor(0); // set draw color to black
      // u8g2.drawBox(0, 10, 20, 30);
      // u8g2.setDrawColor(1); // reset draw color to white
    }
    u8g2.sendBuffer(); // transfer internal memory to the display
    delay(5000);
  }

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ lifecycle ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  blueToggler->checkToggleLED();
  extLoadToggler->checkToggleLED();
  consolePrintLifeSign->checkConsolePrint();
}

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ BUSINESS LOGIC FUNCTIONS ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */

/* ...
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

// Scan for devices on the OneWire bus.
// • prints addresses of detected devices to Serial console
// • writes the address of the LAST DEVICE found to `tempSensorDeviceAddress`
// • returns number of devices found
uint8_t scanDevicesAddressesAndRememberLast(OneWire &bus, DeviceAddress addressOut) {
  uint8_t count = 0;

  if (bus.search(addressOut)) {
    Serial.println(F("Devices with addresses found on OneWire bus:"));
    do {
      count++;
      Serial.print("   ");
      printDeviceAddress(addressOut);
      Serial.println("");
      // addressOut always contains the last found address
    } while (bus.search(addressOut));
  } else {
    Serial.println(F("No devices found on OneWire bus!"));
  }

  return count;
}

// function to print a OneWire device address in Hexadecimal format
void printDeviceAddress(const DeviceAddress address) {
  for (uint8_t i = 0; i < 8; i++) {
    if (address[i] < 0x10) Serial.print("0");
    Serial.print(address[i], HEX);
    if (i < 7) Serial.print(".");
  }
}

// function to print the temperature for a device
void printTemperature(DallasTemperature &sensors, DeviceAddress deviceAddress) {
  float tempC = sensors.getTempC(deviceAddress);
  if (tempC == DEVICE_DISCONNECTED_C) {
    Serial.println("Error: Could not read temperature data");
    return;
  }
  Serial.print("Temp C: ");
  Serial.print(tempC);
  Serial.print(" Temp F: ");
  Serial.print(DallasTemperature::toFahrenheit(tempC));
}

// Service function: Print two lines of text on the OLED, left-aligned, with configurable text size
// Supports text heights: 10, 12, 13, 14, 15, 18, 20.
// Well readable values are sizes 16 and 20
//
// Examples:
//   oledPrintTwoLines(u8g2, "10 Size: 1234567", "ABCDEFG", 10); delay(5000);
//   oledPrintTwoLines(u8g2, "12 Size: 1234567", "ABCDEFG", 12); delay(5000);
//   oledPrintTwoLines(u8g2, "13 Size: 1234567", "ABCDEFG", 13); delay(5000);
//   oledPrintTwoLines(u8g2, "14 Size: 1234567", "ABCDEFG", 14); delay(5000);
//   oledPrintTwoLines(u8g2, "15 Size: 1234567", "ABCDEFG", 15); delay(5000);
//   oledPrintTwoLines(u8g2, "18 Size: 1234567", "ABCDEFG", 18); delay(5000);
//   oledPrintTwoLines(u8g2, "22 Size: 1234567", "ABCDEFG", 22); delay(5000);
//
void oledPrintTwoLines(U8G2 &display, const char *line1, const char *line2, uint8_t textHeight /* = 16 */) {
  display.clearBuffer();
  uint8_t fontHeight = textHeight;
  uint8_t yOffset = 2;
  uint8_t yPad = 2;

  // Choose a font based on the requested height, suitable for small screens
  if (textHeight <= 10) {
    display.setFont(u8g2_font_5x8_tf);
    fontHeight = 10;
    yOffset = 6;
    yPad = 6;
  } else if (textHeight <= 12) {
    display.setFont(u8g2_font_6x12_tf);
    fontHeight = 10;
    yOffset = 5;
    yPad = 5;
  } else if (textHeight <= 13) {
    display.setFont(u8g2_font_6x13_tf);
    fontHeight = 10;
    yOffset = 4;
    yPad = 6;
  } else if (textHeight <= 14) {
    display.setFont(u8g2_font_7x13_tf);
    fontHeight = 11;
    yOffset = 3;
    yPad = 7;
  } else if (textHeight <= 15) {
    display.setFont(u8g2_font_8x13_tf);
    yOffset = 2;
    yPad = 4;
    fontHeight = 12;
  } else if (textHeight <= 16) {
    display.setFont(u8g2_font_9x15_tf);
    yOffset = 1;
    fontHeight = 13;
  } else if (textHeight <= 18) {
    display.setFont(u8g2_font_9x18_tf);
    fontHeight = 14;
    yOffset = 0;
  } else {
    display.setFont(u8g2_font_10x20_tf);
    fontHeight = 15;
    yPad = 2;
    yOffset = 0;
  }
  // Print first line at top
  display.drawStr(0, yOffset + fontHeight, line1);
  // Print second line below
  display.drawStr(0, yOffset + 2 * fontHeight + yPad, line2);
  display.sendBuffer();
}

// Service function: Scroll a single line of text horizontally on the OLED, with configurable text height and speed.
// The `scrollSpeedMs` is the delay between moving the text by 1 pixel (default: 50ms).
//
// Examples:
//   oledScrollText(u8g2, "Done with setup. Kolibrie commencing operations!", 20, 10);
void oledScrollText(U8G2 &display, const char *text, uint8_t textHeight /* = 16 */, uint16_t scrollSpeedMs /* = 50 */) {
  // Font selection logic (unchanged)
  uint8_t fontHeight = textHeight;
  uint8_t yOffset = 2;
  if (textHeight <= 10) {
    display.setFont(u8g2_font_5x8_tf);
    fontHeight = 10;
    yOffset = 6;
  } else if (textHeight <= 12) {
    display.setFont(u8g2_font_6x12_tf);
    fontHeight = 10;
    yOffset = 5;
  } else if (textHeight <= 13) {
    display.setFont(u8g2_font_6x13_tf);
    fontHeight = 10;
    yOffset = 4;
  } else if (textHeight <= 14) {
    display.setFont(u8g2_font_7x13_tf);
    fontHeight = 11;
    yOffset = 3;
  } else if (textHeight <= 15) {
    display.setFont(u8g2_font_8x13_tf);
    yOffset = 2;
    fontHeight = 12;
  } else if (textHeight <= 16) {
    display.setFont(u8g2_font_9x15_tf);
    yOffset = 1;
    fontHeight = 13;
  } else if (textHeight <= 18) {
    display.setFont(u8g2_font_9x18_tf);
    fontHeight = 14;
    yOffset = 0;
  } else {
    display.setFont(u8g2_font_logisoso30_tr);
    fontHeight = 30;
    yOffset = 0;
  }

  display.setFontMode(1); // transparent mode for speed
  uint8_t y = fontHeight + yOffset;
  int textWidth = display.getUTF8Width(text);
  int screenWidth = display.getDisplayWidth();

  int offset = 0;
  while (true) {
    display.clearBuffer();
    int x = offset;
    // Draw text repeatedly for seamless loop
    do {
      display.drawUTF8(x, y, text);
      x += textWidth;
    } while (x < screenWidth);
    display.sendBuffer();
    offset--;
    if (offset < -textWidth) offset = 0;
    delay(scrollSpeedMs);
    // Optionally break after one full scroll (uncomment below)
    if (offset == 0) break;
  }
}
