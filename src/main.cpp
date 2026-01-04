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
#include "ErrDisplay.h"
#include "ErrorMessages.h"
#include "FrequentlyUtils.h"
#include "LedUtils.h"
#include "StatDisplay.h"

// Preprocessor Macros
#define DEBUG // extended behavior for debugging (e.g., Serial console output, delayed operations for observability, etc.)

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ System CONFIGURATION ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */
// Wifi credentials:
#include "WiFiCredentials.h"

/* On-Board Screen (OLED 72x40)
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

static U8G2_SSD1306_72X40_ER_F_SW_I2C u8g2(U8G2_R2, 6, 5, U8X8_PIN_NONE);
// U8G2_R0 	No rotation, landscape
// U8G2_R1 90 degree clockwise rotation
// U8G2_R2 180 degree clockwise rotation
// U8G2_R3 270 degree clockwise rotation

/* DS18B20 Temperature Sensor
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
static constexpr uint8_t TEMPERATURE_SENSOR_GPIO = 2; // DS18B20 is connected to GPIO 2; this is the port for the OneWire bus
static constexpr uint8_t TEMPERATURE_PRECISION = 10;  // select 10 bit precision for DS18B20 (available range is 9 to 12 bits): corresponds to 0.25°C resolution with 187.5 ms measurement duration
static OneWire temperatureSensorBus(TEMPERATURE_SENSOR_GPIO);
static DallasTemperature temperatureSensors(&temperatureSensorBus);

// Reading temperatures with the DallasTemperature library is a two setp process for efficiency:
// 1. Request temperature measurement (non-blocking, when `waitForConversion` is set to false) via
//    methods `requestTemperaturesByAddress` or `requestTemperatures` or `requestTemperaturesByIndex`
// 2. After sufficient time has passed for the measurement to complete, retrieve the temperature via
//    `getTempC` or `getTempF`. In our case, the waittime is at least 187.5ms, as we use 10-bit precision.
static FrequencyTrigger requestTempRead(FrequencyUtils::unbounded_lifetime, 1000u); // read temperature every 1s, unbounded lifetime
static CooldownTriggerN retrieveTemp(1, 220u);                                      // wait at least 220ms after requesting temperature read to retrieve it and manually deactivate

/* LEDs
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
static constexpr uint8_t BLUE_LED_BUILTIN = 8; // GPIO 8, Blue LED: LOW = on, HIGH = off

// LEDs' blinking patterns to indicate that temperature was measured successfully
// blinking patter ("-" denoting LED on for 500ms, "." denoting LED off for 200ms):  - . -
// ⇒ lifetime 1200ms for single measurement success indication
// This is only activated after the temperature was successfully measured.
static LEDExpiringToggler tempMeasurementSuccess(BLUE_LED_BUILTIN, 1200, 500, 200, LedUtils::LOW_IS_ON);

/* Controller for External Load -> GPIO
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
// EXT_LOAD_SWITCH defines the GPIO that is used to control an external load attached.
// EXT_LOAD_ON and EXT_LOAD_OFF define the states that correspond to the load being provided
// power or not. Here, the micro-controller's GPIO (3.3V) controls the external load, but
// through an IRL530 Power Mosfet, supplying 5V trigger to a Solid-State-Relay switching AC mains.
static constexpr uint8_t EXT_LOAD_SWITCH = 1; // GPIO 1 controls the external load (through a Mosfet supplying 5V trigger to SSR switching AC mains)
#define EXT_LOAD_ON HIGH
#define EXT_LOAD_OFF LOW

// For testing purposes, we are "misusing" an LED toggler to control the external load logic
LEDExpiringToggler *extLoadToggler = nullptr; // TODO: remove

/* IO and APIs
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
// prints life-signs to Serial console, unbounded runtime, print every 7331 milliseconds.
// Notes:
//  * Can be reassigned to display different messages during runtime
//    CAUTION: heap allocation. Frequent reassignment can cause heap fragmentation on MCUs. Reassign sparingly.
//  * We choose an larger prime interval to avoid accidental synchronization with other periodic tasks
auto consolePrintLifeSign = std::make_unique<PrintLifeSign>(FrequencyUtils::unbounded_lifetime, 7331, "Controller alive");

// Application-specific Status Displays
static StatDisplay statDisplay(u8g2, 700, 300);

// Generic Error Display (for displaying error messages on the OLED)
std::unique_ptr<ErrDisplay> errDisplay = nullptr;

/* Misc
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

int64_t startMicros = 0;
int testStateCounter = 0;

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ CONTROLLER INITIALIZATION ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */

/* FUNCTION PROTOTYPES
 * ╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴ */
uint8_t scanDevicesAddressesAndRememberLast(OneWire &bus, DeviceAddress addressOut);
float initTemperatureSensor();
float printTemperature(DallasTemperature &sensors, DeviceAddress deviceAddress);
float readTemp(DallasTemperature &sensors, DeviceAddress deviceAddress);

void printDeviceAddress(const DeviceAddress address);
void displayErrorAndHalt(const String &errorMessage);

/* FRAMEWORK FUNCTION setup(): called by Arduino framework once at startup
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
void setup() {
  Serial.begin(115200);
#if defined(DEBUG)
  delay(1000); // provide some time for Monitor to connect
#endif

  Serial.println(F("Hello, blink blink blink ;-)\n"));

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ LEDs ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  // STARTUP BLINKER:: signals is starting up
  // blinking patter ("-" denoting LED on for 150ms, "." denoting LED off for 100ms):  -. -. -. -
  // ⇒ lifetime 900ms
  { // stack allocated (no heap fragmentation):
    LEDExpiringToggler startupBlinker(BLUE_LED_BUILTIN, 900, 150, 100, LedUtils::LOW_IS_ON);
    startupBlinker.activate();
    while (true) {
      delay(5);
      startupBlinker.checkToggleLED();
      if (startupBlinker.isExpired()) break;
    }
  } // startupBlinker on stack automatically destroyed here when leaving scope

  /* LEDs' blinking patterns to indicate that temperature was measured successfully
   * blinking patter ("-" denoting LED on for 500ms, "." denoting LED off for 200ms):  - . -
   * ⇒ lifetime 1200ms for single measurement success indication
   * This is only activated after the temperature was successfully measured */

  /* ── Toggling GPIO 1, which connects to Mosfet ─────────── */
  extLoadToggler = new LEDExpiringToggler(EXT_LOAD_SWITCH, -1, 2000, LedUtils::HIGH_IS_ON); // toggles every 2 seconds

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Setup On-Board Screen (OLED 72x40) ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setBusClock(400000); // 400kHz I2C

  u8g2.enableUTF8Print();
  u8g2.setFontMode(0); // enable transparent mode, which is faster

  // contrast (i.e. brightness) on OLED displays is controlled by the current supplied to the organic light-emitting diodes.
  // Range: 0 (no contrast) to 255 (maximum contrast or brightness).
  u8g2.setContrast(3); // set contrast to maximum

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ DS18B20 Temperature Sensor ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */

  float tempC = initTemperatureSensor();
  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Happy Path Status Display ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */

  statDisplay.setHeatingStatus(false);
  statDisplay.setWifiStatus(false);
  statDisplay.setTemp(tempC);

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ start ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  consolePrintLifeSign->activate(293);

  requestTempRead.activate();

  tempMeasurementSuccess.activate();
  extLoadToggler->activate();

  requestTempRead.activate(421);

  Serial.println(F("Done with setup. Kolibrie commencing operations!\n"));
  startMicros = esp_timer_get_time(); // Initialize global startMicros for loop() timing checks
}

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ CONTROLLER LOOP ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */
/*
 * ╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴ */

void loop() { /* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
  int64_t currentMicros = esp_timer_get_time();

  // Serial.print(F("  Current temperature: "));
  // Serial.print(tempC);
  // Serial.print(F(" C / "));

  // errDisplay->checkRedraw(currentMicros);

  // statDisplay->checkRedraw(currentMicros);

  // if ((currentMicros - startMicros > 10000000) && (testStateCounter == 0)) {
  //   statDisplay->setHeatingStatus(false);
  //   statDisplay->setTemp(1.23);
  //   Serial.println(F("transitioning 1 -> 2"));
  //   testStateCounter = 1;
  // }

  // if ((currentMicros - startMicros > 20000000) && (testStateCounter == 1)) {
  //   statDisplay->setWifiStatus(false);
  //   statDisplay->setTemp(-17.1);
  //   Serial.println(F("transitioning 2 -> 3"));
  //   testStateCounter = 2;
  // }

  // if ((currentMicros - startMicros > 30000000) && (testStateCounter == 2)) {
  //   statDisplay->setWifiStatus(true);
  //   statDisplay->setTemp(-3.4);
  //   Serial.println(F("transitioning 3 -> 4"));
  //   testStateCounter = 3;
  // }

  // if ((currentMicros - startMicros > 40000000) && (testStateCounter == 3)) {
  //   statDisplay->setHeatingStatus(true);
  //   statDisplay->setTemp(-0.4);
  //   Serial.println(F("transitioning 4 -> 5"));
  //   testStateCounter = 4;
  // }

  consolePrintLifeSign->checkConsolePrint(currentMicros);
}

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ BUSINESS LOGIC FUNCTIONS ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */

/* ...
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ DS18B20 Temperature Sensor ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

// Display error message on both Serial console and OLED, then halt execution.
// It is recommended to start the error message with a leading blank. This helps when scrolling text,
// providing a space between the line leaving the display and the repeated message scrolling into the display.
void displayErrorAndHalt(const String &errorMessage) {
  // Print to Serial console
  int64_t lastSerialPrintMicros = esp_timer_get_time();
  Serial.println();
  Serial.print(F("ERROR:")); // leading blank of `errorMessage` is provided by the caller
  Serial.println(errorMessage);
  Serial.println(F("Halting execution."));

  // Create and activate error display
  DisplayText headline = DisplayText(u8g2, String(F(" ERROR")), 20);
  DisplayText detailedMsg = DisplayText(u8g2, errorMessage, 16);
  errDisplay = std::make_unique<ErrDisplay>(u8g2, headline, detailedMsg, 20);

  // Infinite loop: keep updating the error display
  int64_t currentMicros;
  while (true) {
    // The error display needs to be called very frequently for smooth scrolling. In contrast, the console print of the error message is
    // much less frequent and not particularly time sensitive. Since the check involves expensive 64-bit integer arithmetic, we check it
    // separately infrequently (every 773 iterations) to avoid doing the expensive 64-bit integer check every time.
    for (int i = 772; i >= 0; i--) {
      currentMicros = esp_timer_get_time();
      errDisplay->checkRedraw(currentMicros);
    }
    if (lastSerialPrintMicros + 7000000LL > currentMicros) { // only print every 7 seconds to avoid flooding Serial console
      lastSerialPrintMicros = currentMicros;
      Serial.print(F("ERROR:")); // leading blank of `errorMessage` is provided by the caller
      Serial.println(errorMessage);
      Serial.println(F("Halted execution."));
    }
  }
}

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
      Serial.print(F("   "));
      printDeviceAddress(addressOut);
      Serial.println();
      // addressOut always contains the last found address
    } while (bus.search(addressOut));
  } else {
    Serial.println(F("No devices found on OneWire bus!"));
  }

  return count;
}

// function to print a OneWire device address in Hexadecimal format XX.XX.XX.XX.XX.XX.XX.XX
// This function entirely avoids any heap allocations. This is achieved by using the Serial.print()
// function directly byte by byte, without intermediate string construction. Arduino's Print class
// (which Serial inherits from) is specifically designed to avoid dynamic allocation for primitive
//  data types like uint8_t and chars.
void printDeviceAddress(const DeviceAddress address) {
  for (uint8_t i = 0; i < 8; i++) {
    if (address[i] < 0x10) Serial.print('0');
    Serial.print(address[i], HEX);
    if (i < 7) Serial.print('.');
  }
}

/* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ DS18B20 Temperature Sensor ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */

/* initTemperatureSensor scans the the OneWire and attempts to connect to the DS18B20 temperature sensor,
 * which is expected to be the only device on the bus. We verify the device is a compatible temperature
 * sensor by checking its address family code. In case of any unexpected conditions, this function will
 * print an error message to the Serial console, print an error on the OLED display, and halt execution.
 *
 * CAUTION: this function reads and writes (intention: initialization) globally defined variables:
 *  • The sensor is initialized with the precision defined by the `TEMPERATURE_PRECISION` constant.
 *    Currently: 0.25°C resolution requiring 187.5 ms measurement duration
 *  • The address of the sensor is stored in the global variable `tempSensorDeviceAddress`.
 *
 */
float initTemperatureSensor() {
  Serial.print(F("Scanning for OneWire devices on GPIO pin "));
  Serial.println(TEMPERATURE_SENSOR_GPIO, DEC);
  DeviceAddress tempSensorDeviceAddress; // type definition for DS18B20 address (8 bytes), provided by DallasTemperature library

  // STEP 1: scan for connected devices on the OneWire bus:
  uint8_t deviceCount = scanDevicesAddressesAndRememberLast(temperatureSensorBus, tempSensorDeviceAddress);
  if (deviceCount != 1) {
    ErrorMessages::DeviceCountError::build(ErrorMessages::errorBuffer, TEMPERATURE_SENSOR_GPIO, deviceCount);
    displayErrorAndHalt(String(ErrorMessages::getBuffer()));
  }

  // STEP 2: Pre-Init Sanity Checks that the device is supported by the DallasTemperature driver:
  Serial.print(F("Verifying that sensor at address "));
  printDeviceAddress(tempSensorDeviceAddress);
  Serial.println(F(" is supported by the DallasTemperature driver."));

  if (!(temperatureSensors.validAddress(tempSensorDeviceAddress))) { // confirm that the address is valid
    ErrorMessages::IncompatibleAddressError::build(ErrorMessages::errorBuffer, tempSensorDeviceAddress);
    displayErrorAndHalt(String(ErrorMessages::getBuffer()));
  }
  if (!(temperatureSensors.validFamily(tempSensorDeviceAddress))) { // confirm the device is supported by the driver
    ErrorMessages::UnknownDeviceError::build(ErrorMessages::errorBuffer, tempSensorDeviceAddress);
    displayErrorAndHalt(String(ErrorMessages::getBuffer()));
  }

  // STEP 3: Init Temperature Sensor
  temperatureSensors.begin();                     // Initialize the sensor.
  temperatureSensors.setWaitForConversion(false); // makes `sensors.requestTemperaturesByAddress` non-blocking, need to track time manually after requesting temperature conversion

  // STEP 4: Post-Init Sanity Checks that the DS18B20 temperature sensor is properly connected
  if (!(temperatureSensors.isConnected(tempSensorDeviceAddress))) { // sanity check: the device at the expected address is reported as connected
    ErrorMessages::SensorDisconnectedError::build(ErrorMessages::errorBuffer, tempSensorDeviceAddress);
    displayErrorAndHalt(String(ErrorMessages::getBuffer()));
  }

  // Check that sensor is not reporting parasite power mode. Parasite power mode is not expected and likely a symptom of some defect.
  if (temperatureSensors.readPowerSupply(tempSensorDeviceAddress)) { // Read device's power requirements. Return 1 if device needs parasite power.
    // Serial.print(F("WARNING: DS18B20 temperature sensor "));
    // printDeviceAddress(tempSensorDeviceAddress);
    // Serial.println(F(" is reporting PARASITE POWER MODE. This is unexpected and may indicate a defect."));
    ErrorMessages::ParasitePowerError::build(ErrorMessages::errorBuffer, tempSensorDeviceAddress);
    displayErrorAndHalt(String(ErrorMessages::getBuffer()));
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
    ErrorMessages::PrecisionSettingError::build(ErrorMessages::errorBuffer, tempSensorDeviceAddress, TEMPERATURE_PRECISION, actualPrecision);
    displayErrorAndHalt(String(ErrorMessages::getBuffer()));
  }

  // Happy path
  Serial.print(F("Sensor operating with precision of "));
  Serial.print(actualPrecision, DEC);
  Serial.print(F(" bits."));
  float tempC = readTemp(temperatureSensors, tempSensorDeviceAddress);
  if (!isfinite(tempC)) {
    ErrorMessages::TemperatureReadError::build(ErrorMessages::errorBuffer, tempC);
    displayErrorAndHalt(String(ErrorMessages::getBuffer()));
  }
  Serial.print(F("  Current temperature: "));
  Serial.print(tempC);
  Serial.print(F(" C / "));
  Serial.print(DallasTemperature::toFahrenheit(tempC));
  Serial.print(F(" F"));
  Serial.println();
  Serial.println(F("DS18B20 temperature sensor successfully initialized"));
  Serial.println();

  while (true) {
    printTemperature(temperatureSensors, tempSensorDeviceAddress);
    Serial.println();
    delay(1000);
  }

  return tempC;
}

// printTemperature
// • measured the temperature,
// • prints the temperature to Serial console in units of Celsius and Fahrenheit
// • in case of error (e.g., sensor disconnected), an error message is printed instead
// • returns the temperature in Celsius as float; in case of error, NaN is returned
float printTemperature(DallasTemperature &sensors, DeviceAddress deviceAddress) {
  // Request temperature conversion and wait for it to complete
  sensors.requestTemperaturesByAddress(deviceAddress);
  // Wait for conversion to complete (at 10-bit: ~187.5ms)
  delay(200); // TODO nonblocking

  float tempC = sensors.getTempC(deviceAddress);
  if (tempC == DEVICE_DISCONNECTED_C) {
    Serial.println();
    Serial.println(F("ERROR: reading temperature failed with value "));
    return NAN;
  }
  Serial.print(tempC);
  Serial.print(F(" C / "));
  Serial.print(DallasTemperature::toFahrenheit(tempC));
  Serial.print(F(" F"));
  return tempC;
}

// readTemp measures the temperature, returns the temperature in Celsius as float
// or NAN in case of error
float readTemp(DallasTemperature &sensors, DeviceAddress deviceAddress) {
  // Request temperature conversion and wait for it to complete
  sensors.requestTemperaturesByAddress(deviceAddress);
  // Wait for conversion to complete (at 10-bit: ~187.5ms)
  delay(200); // TODO nonblocking

  float tempC = sensors.getTempC(deviceAddress);
  if (tempC == DEVICE_DISCONNECTED_C) {
    return NAN;
  }
  return tempC;
}

// NOTEs:
// example for displaying temperature on web server hosted by the MCU: https://randomnerdtutorials.com/esp32-ds18b20-temperature-arduino-ide/