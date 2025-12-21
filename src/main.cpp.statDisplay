#include <Arduino.h>
#include <U8g2lib.h>
#include <memory>

// WIFI
#include <WiFi.h>
#include <WiFiClientSecure.h>

// DS18B20 Temperature Sensor Libraries
#include "DallasTemperature.h"
#include "OneWire.h"

// Custom utils
#include "ConsoleUtils.h"
#include "Display.h"
#include "FrequentlyUtils.h"
#include "LedUtils.h"
#include "StatDisplay.h"

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
std::unique_ptr<FrequencyTrigger> readTriggerTemperature = nullptr;

/* LEDs
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
#define BLUE_LED_BUILTIN 8 // GPIO 8, Blue LED: LOW = on, HIGH = off

// LED Blinking patterns to indicate current state
LEDExpiringToggler *blueToggler = nullptr; // blinks 5 times turning 1 second

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

std::unique_ptr<FrequencyToggler> extLoadOnDisplayBlinker = nullptr;

/* IO and APIs
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
// prints life-signs to Serial console, unbounded runtime, print every 5000 milliseconds
// Note: Static allocation avoids memory leak; object persists for application lifetime
static PrintLifeSign consolePrintLifeSignInstance(FrequencyUtils::unbounded_lifetime, 5000, "Controller alive");
PrintLifeSign *consolePrintLifeSign = &consolePrintLifeSignInstance;

std::unique_ptr<StatDisplay> statDisplay = nullptr;

std::unique_ptr<DisplayScrollText> oledLine1Scroller = nullptr;
std::unique_ptr<DisplayScrollText> oledLine1Scroller2 = nullptr;

int64_t startMicros = 0;
int testStateCounter = 0;

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ CONTROLLER INITIALIZATION ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */

/* FUNCTION PROTOTYPES
 * ╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴ */
uint8_t scanDevicesAddressesAndRememberLast(OneWire &bus, DeviceAddress addressOut);
void printDeviceAddress(const DeviceAddress address);
void printTemperature(DallasTemperature &sensors, DeviceAddress deviceAddress);

// void oledPrintTwoLines(U8G2 &display, const char *line1, const char *line2, uint8_t textHeight = 16);
// uint8_t oledScrollText(U8G2 &display, const String &text, uint8_t yOffset, uint8_t textHeight /* = 16 */, uint16_t scrollSpeedMs /* = 50 */);
// uint8_t oledPrintSingleLine(U8G2 &display, const String &line, uint8_t yOffset, uint8_t textHeight /* = 16 */);

/* FRAMEWORK FUNCTION setup(): called by Arduino framework once at startup
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

void setup() { /* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
  Serial.begin(115200);
  delay(1000);

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ On-Board Screen (OLED 72x40) ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setBusClock(400000); // 400kHz I2C

  u8g2.enableUTF8Print();
  u8g2.setFont(u8g2_font_logisoso30_tf); // set the target font to calculate the pixel width
  u8g2.setFontMode(0);                   // enable transparent mode, which is faster

  // contrast (i.e. brightness) on OLED displays is controlled by the current supplied to the organic light-emitting diodes.
  // Range: 0 (no contrast) to 255 (maximum contrast or brightness).
  u8g2.setContrast(3); // set contrast to maximum

  statDisplay = std::make_unique<StatDisplay>(u8g2, 700, 300);
  statDisplay->setHeatingStatus(true);
  // statDisplay->setWifiStatus(true);
  statDisplay->setTemp(-88.52);

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

  readTriggerTemperature = std::make_unique<FrequencyTrigger>(FrequencyUtils::unbounded_lifetime, 5000u); // read temperature every 5s, unbounded lifetime
  extLoadOnDisplayBlinker = std::make_unique<FrequencyToggler>(FrequencyUtils::unbounded_lifetime, 500u); // blinks every 500ms when activated

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
  // Reuse the same toggler instance with new configuration (avoiding memory leak from prior allocation)
  delete blueToggler;
  blueToggler = new LEDExpiringToggler(BLUE_LED_BUILTIN, -1, 2000, LedUtils::LOW_IS_ON); // blinks every 2 seconds

  /* ── Toggling GPIO 1, which connects to Mosfet ─────────── */
  extLoadToggler = new LEDExpiringToggler(EXT_LOAD_SWITCH, -1, 2000, LedUtils::HIGH_IS_ON); // toggles every 2 seconds

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ start ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  blueToggler->activate();
  extLoadToggler->activate();

  consolePrintLifeSign->activate(293);
  readTriggerTemperature->activate(421);
  extLoadOnDisplayBlinker->activate(421);

  // line print demonstration
  // - top line in 16 pt
  // - bottom line in 14 pt
  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ start ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  int64_t localStartMicros = esp_timer_get_time();
  int64_t currentMicros = startMicros;
  String line1 = "AgCDEFG";
  DisplayText line2 = DisplayText(u8g2, "1T3q567", 16);
  u8g2_uint_t y;
  do {
    u8g2.clearBuffer();
    y = 2;
    y = Display::oledPrintSingleLine(u8g2, line1, y, 14);
    y = Display::oledPrintSingleLine(u8g2, line2, y);
    u8g2.sendBuffer(); // transfer internal memory to the display

    currentMicros = esp_timer_get_time();
  } while (currentMicros - localStartMicros < 500000);

  // oledScrollText(u8g2, "Done with setup. Kolibrie commencing operations!", 20, 10);
  // delay(5000);

  // line print scroll demonstration
  // - top line in 15 pt
  // - bottom line in 18 pt
  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ start ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  y = 2;
  uint8_t textHeight = 15;
  oledLine1Scroller = std::make_unique<DisplayScrollText>(u8g2, text1, y, textHeight, 20);

  uint8_t y2 = oledLine1Scroller->getNextLineYOffset() + 6;
  uint8_t textHeight2 = 18;
  oledLine1Scroller2 = std::make_unique<DisplayScrollText>(u8g2, "Hello World. ", y2, textHeight2, 15);

  oledLine1Scroller->activate();
  oledLine1Scroller2->activate(2000);

  Serial.println(F("Done with setup. Kolibrie commencing operations!"));
  startMicros = esp_timer_get_time(); // Initialize global startMicros for loop() timing checks
}

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ CONTROLLER LOOP ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */
/*
 * ╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴ */

void loop() { /* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
  int64_t currentMicros = esp_timer_get_time();
  statDisplay->checkRedraw(currentMicros);

  if ((currentMicros - startMicros > 10000000) && (testStateCounter == 0)) {
    statDisplay->setHeatingStatus(false);
    statDisplay->setTemp(1.23);
    Serial.println(F("transitioning 1 -> 2"));
    testStateCounter = 1;
  }

  if ((currentMicros - startMicros > 20000000) && (testStateCounter == 1)) {
    statDisplay->setWifiStatus(false);
    statDisplay->setTemp(-17.1);
    Serial.println(F("transitioning 2 -> 3"));
    testStateCounter = 2;
  }

  if ((currentMicros - startMicros > 30000000) && (testStateCounter == 2)) {
    statDisplay->setWifiStatus(true);
    statDisplay->setTemp(-3.4);
    Serial.println(F("transitioning 3 -> 4"));
    testStateCounter = 3;
  }

  if ((currentMicros - startMicros > 40000000) && (testStateCounter == 3)) {
    statDisplay->setHeatingStatus(true);
    statDisplay->setTemp(-0.4);
    Serial.println(F("transitioning 4 -> 5"));
    testStateCounter = 4;
  }

  consolePrintLifeSign->checkConsolePrint(currentMicros);
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
