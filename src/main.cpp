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
typedef uint8_t address_DS18B20[8]; // type definition for DS18B20 address (8 bytes)

#define TEMPERATURE_SENSOR_GPIO 2 // DS18B20 is connected to GPIO 2; this is the port for the OneWire bus
OneWire temperatureSensorBus(TEMPERATURE_SENSOR_GPIO);
DallasTemperature temperatureSensors(&temperatureSensorBus);

address_DS18B20 tempSensorDeviceAddress;
FrequencyTrigger *readTriggerTemperature = nullptr;

/* LEDs
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
#define BLUE_LED_BUILTIN 8 // GPIO 8, Blue LED: LOW = on, HIGH = off

// LED Blinking patterns to indicate current state
LEDExpiringToggler *blueToggler = nullptr; // blinks 5 times turning o1 second

/* Controller Output for External Load -> GPIO
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

/* Life-Signs
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
// prints life-signs to Serial console, unbounded runtime, print every 5000 milliseconds
PrintLifeSign *consolePrintLifeSign = new PrintLifeSign(-1, 5000, "Controller alive");

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ CONTROLLER INITIALIZATION ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */

/* FUNCTION PROTOTYPES
 * ╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴ */
uint8_t scanDevicesAddressesAndRememberLast(OneWire bus);
void printDeviceAddress(const address_DS18B20 address);

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
  uint8_t deviceCount = scanDevicesAddressesAndRememberLast(temperatureSensorBus); // scan for connected DS18B20 devices
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

  temperatureSensors.begin();                              // Start up the library
  readTriggerTemperature = new FrequencyTrigger(-1, 5000); // read temperature every 2s, unbounded lifetime

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ LEDs ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  // blinks quickly every 300ms for a total duration of 1.35s to indicate system is starting up
  blueToggler = new LEDExpiringToggler(BLUE_LED_BUILTIN, 1350, 150, LEDExpiringToggler::LOW_IS_ON);
  blueToggler->activate();
  while (true) {
    delay(20);
    blueToggler->checkToggleLED();
    if (blueToggler->isExpired()) break;
  }

  /* ── LEDs' blinking patterns to indicate current state ─────────── */
  blueToggler = new LEDExpiringToggler(BLUE_LED_BUILTIN, -1, 2000, LEDExpiringToggler::LOW_IS_ON); // blinks 1 times turning o1 second

  /* ── Toggling GPIO 1, which connects to Mosfet ─────────── */
  extLoadToggler = new LEDExpiringToggler(EXT_LOAD_SWITCH, -1, 2000, LEDExpiringToggler::HIGH_IS_ON); // blinks 1 times turning o1 second

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ start ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  blueToggler->activate();
  extLoadToggler->activate();

  consolePrintLifeSign->activate(293);
  readTriggerTemperature->activate(421);
  Serial.println(F("Done with setup. Kolibrie commencing operations!"));
}

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ CONTROLLER LOOP ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */
/*
 * ╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴ */

void loop() { /* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

  u8g2.clearBuffer();                  // clear the internal memory
  u8g2.drawFrame(0, 0, width, height); // draw a frame around the border
  u8g2.setCursor(xOffset + 15, yOffset + 25);
  // u8g2.printf("%dx%d", width, height);
  u8g2.drawUTF8(0, text1_y0, "12"); // draw the scolling text

  u8g2.drawUTF8(42, text1_y0 + 6, "°"); // draw the scolling text
  u8g2.drawUTF8(54, text1_y0, "C");
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
uint8_t scanDevicesAddressesAndRememberLast(OneWire bus) {
  uint8_t count = 0;

  if (bus.search(tempSensorDeviceAddress)) {
    Serial.println(F("Devices with addresses found on OneWire bus:"));
    do {
      count++;
      Serial.print("   ");
      printDeviceAddress(tempSensorDeviceAddress);
      Serial.println("");
    } while (bus.search(tempSensorDeviceAddress));
  } else {
    Serial.println(F("No devices found on OneWire bus!"));
  }

  return count;
}

// function to print a OneWire device address in Hexadecimal format
void printDeviceAddress(const address_DS18B20 address) {
  for (uint8_t i = 0; i < 8; i++) {
    if (address[i] < 0x10) Serial.print("0");
    Serial.print(address[i], HEX);
    if (i < 7) Serial.print(".");
  }
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

// function to print the temperature for a device
void printTemperature(DeviceAddress deviceAddress) {
  float tempC = temperatureSensors.getTempC(deviceAddress);
  if (tempC == DEVICE_DISCONNECTED_C) {
    Serial.println("Error: Could not read temperature data");
    return;
  }
  Serial.print("Temp C: ");
  Serial.print(tempC);
  Serial.print(" Temp F: ");
  Serial.print(DallasTemperature::toFahrenheit(tempC));
}
