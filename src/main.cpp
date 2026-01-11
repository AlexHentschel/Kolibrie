#include <Arduino.h>
#include <U8g2lib.h>
#include <memory>

// WIFI (headers included but not yet used - planned for future network functionality)
#include <WiFi.h>
#include <WiFiClientSecure.h>

// DS18B20 Temperature Sensor Libraries
#include "DallasTemperature.h"
#include "OneWire.h"

// ESP32 Watchdog Timer (TWDT): allows monitoring FreeRTOS tasks and trigger a system reset if a task
// runs too long without yielding, preventing system hangs from infinite loops or blocked code.
#include <esp_task_wdt.h>

// Custom utils
#include "ConsoleUtils.h"
#include "DebugUtils.h"
#include "Display.h"
#include "ErrDisplay.h"
#include "ErrorMessages.h"
#include "Ewma.h"
#include "FrequentlyUtils.h"
#include "LedUtils.h"
#include "StatDisplay.h"

// Preprocessor Macros
#define DEBUG // extended behavior for debugging (e.g., Serial console output, delayed operations for observability, etc.)

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ System CONFIGURATION ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */
// Wifi credentials:
#include "WiFiCredentials.h"

/* On-Board Screen (OLED 72x40)
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

static U8G2_SSD1306_72X40_ER_F_SW_I2C u8g2(U8G2_R2, 6, 5, U8X8_PIN_NONE);
// U8G2_R0 	No rotation, landscape
// U8G2_R1 90 degree clockwise rotation
// U8G2_R2 180 degree clockwise rotation
// U8G2_R3 270 degree clockwise rotation

/* Temperature Control
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

/* DS18B20 Temperature Sensor
 * ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
// • An edge case we have seen is a constant reading of 25.0°C, which is a common default value the registers
//   are initialzed with, but haven't been updated by successful temperature measurements. Common causes are improper
//   wiring / broken sensor, or missing software requests via .
// • For efficiency, we use ASYNCHRONOUS temperature reads, where in response to our non-blocking request, the sensor
//   starts working and with some latency (details below) updates an internal register, which we can read later.
// • Sanity check: DS18B20 valid range is -55°C to +125°C according to datasheet
//   https://www.analog.com/media/en/technical-documentation/data-sheets/ds18b20.pdf
//   If we read values outside of this range, something is wrong (sensor malfunction, connection issue, etc.)
static constexpr float TEMP_SENSOR_LOEWEST_VALID = -55.0f; // if DS18B20 returns a value strictly smaller than this, something is wrong
static constexpr float TEMP_SENSOR_LARGEST_VALID = 125.0f; // if DS18B20 returns a value strictly larger than this, something is wrong

static constexpr uint8_t TEMPERATURE_SENSOR_GPIO = 2; // DS18B20 is connected to GPIO 2; this is the port for the OneWire bus
static constexpr uint8_t TEMPERATURE_PRECISION = 10;  // select 10 bit precision for DS18B20 (available range is 9 to 12 bits): corresponds to 0.25°C resolution with 187.5 ms measurement duration
static OneWire temperatureSensorBus(TEMPERATURE_SENSOR_GPIO);
static DallasTemperature temperatureSensors(&temperatureSensorBus);

static DeviceAddress tempSensorDeviceAddress; // set by the initialization code for DS18B20; the address (8 bytes) is provided by DallasTemperature library

// Reading temperatures with the DallasTemperature library is a two step process for efficiency:
// 1. Request temperature measurement (non-blocking, when `waitForConversion` is set to false) via
//    methods `requestTemperaturesByAddress` or `requestTemperatures` or `requestTemperaturesByIndex`
// 2. After sufficient time has passed for the measurement to complete, retrieve the temperature via
//    `getTempC` or `getTempF`. In our case, the waittime is at least 187.5ms, as we use 10-bit precision.
// We request temperature measurement every 1s, and after a sufficient delay (see `TEMP_READ_DELAY_MS` below), we
// retrieve the result requested temperature.
static constexpr unsigned int REQUEST_TEMP_INTERVAL_MS = 1000u;
static FrequencyTrigger requestTempRead(FrequencyUtils::unbounded_lifetime, REQUEST_TEMP_INTERVAL_MS); // read temperature every 1s, unbounded lifetime

// milliseconds to wait after requesting temperature read before retrieving it.  In our case, the
// waittime is at least 187.5ms, as we use 10-bit precision. We add about 20% margin to be safe.
static constexpr unsigned int TEMP_READ_DELAY_MS = 220u;

// Delayed trigger for retrieving the temperature after requesting.
// CAUTION: `CooldownTriggerN` fires immediately upon activation (unless a delayMs is specified). We want the _first_ trigger _after_ `TEMP_READ_DELAY_MS`.
// Therefore, we specify `TEMP_READ_DELAY_MS` as DELAY when activating this trigger after requesting the temperature read. The cooldown here is irrelevant,
// as we only want to retrieve the temperature once per request.
static CooldownTriggerN retrieveTemp(1, 99999u);

/* Heating control
 * ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
// static constexpr float TEMP_LIMIT_HEATING_ON = 5.0f;  // Temperature [°C] below which heating is turned ON
// static constexpr float TEMP_LIMIT_HEATING_OFF = 8.0f; // Temperature [°C] above which heating is turned OFF

static constexpr float TEMP_LIMIT_HEATING_ON = 3.0f;  // Temperature [°C] below which heating is turned ON
static constexpr float TEMP_LIMIT_HEATING_OFF = 6.0f; // Temperature [°C] above which heating is turned OFF

// EWMA filter for temperature readings, smoothing factor α = 0.02. This corresponds roughly to a time window of 50 samples. Specifically:
// after a step change of the input, it takes about 50 samples to move the ouput approx. 63% of the way from the old to the new value.
// CAUTION: this instance starts with value 0 and should be initialized with the first temperature reading using the `reset` method.
static Ewma tempEwma(0.02);

/* LEDs
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
static constexpr uint8_t BLUE_LED_BUILTIN = 8; // GPIO 8, Blue LED: LOW = on, HIGH = off

// LEDs' blinking patterns to indicate that temperature was measured SUCCESSFULLY.
// The entire lifetime of this blinker is 100ms. It is configured to turn the LED on for 100ms on and then keep it off
// for the next 500ms (and repeat). Due to the short lifetime, in practice this translates to a single blink of 100ms.
static LEDExpiringToggler tempMeasurementSuccess(BLUE_LED_BUILTIN, 100, 100, 500, LedUtils::LOW_IS_ON);

/* Controller for External Load -> GPIO
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
// EXT_LOAD_SWITCH defines the GPIO that is used to control an external load attached.
// EXT_LOAD_ON and EXT_LOAD_OFF define the states that correspond to the load being provided
// power or not. Here, the micro-controller's GPIO (3.3V) controls the external load, but
// through an IRL530 Power Mosfet, supplying 5V trigger to a Solid-State-Relay switching AC mains.
static constexpr uint8_t EXT_LOAD_SWITCH = 1; // GPIO 1 controls the external load (through a Mosfet supplying 5V trigger to SSR switching AC mains)
#define EXT_LOAD_ON HIGH
#define EXT_LOAD_OFF LOW

// For testing purposes, we are "misusing" an LED toggler to control the external load logic
// Note: This is for testing only. Actual heating control is in the temperature control logic (loop function).
// Stack-allocated to avoid memory leak.
// Toggling GPIO 1, which connects to Mosfet
static LEDExpiringToggler extLoadToggler(EXT_LOAD_SWITCH, -1, 2000, LedUtils::HIGH_IS_ON); // toggles every 2 seconds

/* IO and APIs
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
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
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
// Heap monitoring for debugging and long-term stability tracking
// Prints free heap memory approximately every 10 minutes to detect potential memory leaks or fragmentation.
// We use an a primer number as time interval to avoid synchronization with other periodic tasks.
// static constexpr unsigned int HEAP_MONITOR_INTERVAL_MS = 602143u; // terigger every 10mins and 2.143s
static constexpr unsigned int HEAP_MONITOR_INTERVAL_MS = 60133u; // terigger every 1mins and 133ms
static FrequencyTrigger heapMonitor(FrequencyUtils::unbounded_lifetime, HEAP_MONITOR_INTERVAL_MS);

int64_t startMicros = 0;
int testStateCounter = 0;

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ CONTROLLER INITIALIZATION ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */

/* FUNCTION PROTOTYPES
 * ╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴╴ */
uint8_t scanDevicesAddressesAndRememberLast(OneWire &bus, DeviceAddress addressOut);
float initTemperatureSensor();
void printTemperature(float tempC, bool printFahrenheit /* = false */);
float readTemp(DallasTemperature &sensors, DeviceAddress deviceAddress);

void printDeviceAddress(const DeviceAddress address);
void displayErrorAndHalt(const String &errorMessage);
void inline debug_print_millis_since_startup(int64_t currentMicros);

/* FRAMEWORK FUNCTION setup(): called by Arduino framework once at startup
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
void setup() {
  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Serial and GPIO Initialization ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  Serial.begin(115200);
  pinMode(BLUE_LED_BUILTIN, OUTPUT);
  pinMode(EXT_LOAD_SWITCH, OUTPUT);

  digitalWrite(EXT_LOAD_SWITCH, EXT_LOAD_OFF); // Ensure heating is off at startup

  debug_do([]() { // allows some time for Serial Monitor to connect
    delay(1000);
  });
  Serial.println(F("Hello, blink blink blink ;-)\n"));

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ LEDs ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
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

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Setup On-Board Screen (OLED 72x40) ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setBusClock(400000); // 400kHz I2C

  u8g2.enableUTF8Print();
  u8g2.setFontMode(0); // enable transparent mode, which is faster

  // contrast (i.e. brightness) on OLED displays is controlled by the current supplied to the organic light-emitting diodes.
  // Range: 0 (no contrast) to 255 (maximum contrast or brightness).
  u8g2.setContrast(3); // set contrast to very low (3 out of 255) to preserve display lifespan and reduce power consumption

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Temperature Control ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  float currentTempC = initTemperatureSensor(); // initialize DS18B20 temperature sensor and read current temperature
  tempEwma.reset(currentTempC);                 // initialize EWMA to start at the initial temperature reading

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Happy Path Status Display ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */

  statDisplay.setHeatingStatus(false);
  statDisplay.setWifiStatus(false);
  statDisplay.setTemp(currentTempC);

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Watchdog Timer ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  // Initialize watchdog timer with 10 second timeout for system stability, panic on timeout.
  // This ensures the system will reset if the main loop hangs for any reason.
  // Notes (based on https://forum.arduino.cc/t/watchdog-reset-esp32-if-stuck-more-than-120-seconds/1266565/2 ):
  // • There's no need to call `esp_task_wdt_reset` from `loop`, as long as we call `enableLoopWDT` from `setup`. This is because
  //   the wrapper in `main.cpp` that calls `setup` and `loop` also calls `esp_task_wdt_reset` automatically on every `loop` iteration.
  //   https://github.com/espressif/arduino-esp32/blob/2.0.17/tools/sdk/esp32/include/esp_system/include/esp_task_wdt.h#L45
  // • Calling `enableLoopWDT` will automatically add the current task (which subsequently will continue on to executing the
  //   `loop` function) to the watchdog. So we don't need to call `esp_task_wdt_add(NULL)` here.
  // • Function `esp_task_wdt_init(const esp_task_wdt_config_t *config)` takes a pointer to a configuration struct, but does not
  //   store that specific object's pointer. Therefore, it's safe to pass a pointer to a stack-allocated struct here.
  esp_task_wdt_config_t wdtConfig = {
      .timeout_ms = 10000,
      .trigger_panic = true,
  };
  esp_task_wdt_init(&wdtConfig); // safe to pass pointer to stack allocated struct
  enableLoopWDT();               // enable the watchdog to be reset automatically at the beginning of each `loop` iteration

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ start ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  // Start timing of various tasks. We choose a prime numbers for startup delays with sufficient
  // gaps in order to avoid tasks triggering too closely to each other.
  consolePrintLifeSign->activate(startMicros, 293u);
  requestTempRead.activate(startMicros, 661u);
  heapMonitor.activate(startMicros, 1277u); // heap report about 600ms after requesting temperature read

  // For testing: activate the external load toggler (this simulates heating on/off for testing)
  // In production, remove this and rely only on the temperature-based heating control in loop()
  // extLoadToggler.activate();

  startMicros = esp_timer_get_time(); // Initialize global startMicros for loop() timing checks
  Serial.println(F("Done with setup. Kolibrie commencing operations!\n"));
}

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ CONTROLLER LOOP ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
// Task Watchdog Timer (TWDT) automatically reset when entering the `loop` function by the framework.
void loop() {
  int64_t currentMicros = esp_timer_get_time();

  // Requesting temperature measurement:
  if (requestTempRead.checkTrigger(currentMicros)) {
    temperatureSensors.requestTemperaturesByAddress(tempSensorDeviceAddress);
    retrieveTemp.activate(currentMicros, TEMP_READ_DELAY_MS); // configured to auto-expire after a single trigger
    debug_do([&]() {
      Serial.print((currentMicros - startMicros) / 1000LL);
      Serial.println(F("\trequesting temp measurement"));
    });
  }

  // Retrieving temperature measurement result and processing it:
  if (retrieveTemp.checkTrigger(currentMicros)) {
    float tempC = readTemp(temperatureSensors, tempSensorDeviceAddress); // sanity check inside
    debug_do([&]() {
      Serial.print((currentMicros - startMicros) / 1000LL);
      Serial.print(F("\ttemp reading: "));
      printTemperature(tempC, true);
      Serial.println();
    });

    float smoothedTemp = tempEwma.update(tempC);
    statDisplay.setTemp(smoothedTemp);

    // Heating control logic with hysteresis to avoid rapid cycling
    // We don't change state if temperature is between the two limits
    if (smoothedTemp < TEMP_LIMIT_HEATING_ON) { // Temperature too low: turn heating ON
      digitalWrite(EXT_LOAD_SWITCH, EXT_LOAD_ON);
      statDisplay.setHeatingStatus(true);
    } else if (smoothedTemp > TEMP_LIMIT_HEATING_OFF) { // Temperature high enough: turn heating OFF
      digitalWrite(EXT_LOAD_SWITCH, EXT_LOAD_OFF);
      statDisplay.setHeatingStatus(false);
    }

    tempMeasurementSuccess.activate();
  }

  // Serial.print(F("  Current temperature: "));
  // Serial.print(tempC);
  // Serial.print(F(" C / "));

  statDisplay.checkRedraw(currentMicros);
  tempMeasurementSuccess.checkToggleLED(currentMicros);

  // For testing only: uncomment to enable toggling of external load for testing purposes
  // In production, the heating is controlled by the temperature logic above
  // extLoadToggler.checkToggleLED(currentMicros);

  consolePrintLifeSign->checkConsolePrint(currentMicros);

  // Heap monitoring for debugging and detecting memory leaks over long operation periods
  if (heapMonitor.checkTrigger(currentMicros)) {
    Serial.print((currentMicros - startMicros) / 1000LL);
    Serial.print(F(" HEAP MONITOR: "));
    Serial.print(ESP.getFreeHeap());
    Serial.print(F(" bytes of free heap; "));
    Serial.print(ESP.getMinFreeHeap());
    Serial.println(F(" bytes of minimum free heap recorded since startup"));
  }
}

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ BUSINESS LOGIC FUNCTIONS ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */

/* ...
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ DS18B20 Temperature Sensor ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

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
    // separately infrequently (every 21 iterations) to avoid doing the expensive 64-bit integer check every time.
    for (int i = 20; i >= 0; i--) {
      currentMicros = esp_timer_get_time();
      errDisplay->checkRedraw(currentMicros);
    }
    if (lastSerialPrintMicros + 7000000LL < currentMicros) { // only print every 7 seconds to avoid flooding Serial console
      lastSerialPrintMicros = currentMicros;
      Serial.print(F("ERROR:")); // leading blank of `errorMessage` is provided by the caller
      Serial.println(errorMessage);
      Serial.println(F("Halted execution."));
    }
  }
}

// Scan for devices on the OneWire bus.
// • prints addresses of detected devices to Serial console
// • writes the address of the LAST DEVICE found to `addressOut`
// • returns number of devices found
uint8_t scanDevicesAddressesAndRememberLast(OneWire &bus, DeviceAddress addressOut) {
  uint8_t count = 0;

  if (bus.search(addressOut)) {
    Serial.println(F("Device(s) on OneWire bus found with addresses:"));
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

/* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ DS18B20 Temperature Sensor ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */

/* initTemperatureSensor scans the the OneWire and attempts to connect to the DS18B20 temperature sensor,
 * which is expected to be the only device on the bus. We verify the device is a compatible temperature
 * sensor by checking its address family code. In case of any unexpected conditions, this function will
 * print an error message to the Serial console, print an error on the OLED display, and halt execution.
 *
 * CAUTION: this function reads and writes (intention: initialization) globally defined variables:
 *  • The sensor is initialized with the precision defined by the `TEMPERATURE_PRECISION` constant.
 *    Currently: 0.25°C resolution requiring 187.5 ms measurement duration
 *  • The address of the sensor is stored in the global variable `tempSensorDeviceAddress`.
 */
float initTemperatureSensor() {
  Serial.print(F("Scanning for OneWire devices on GPIO pin "));
  Serial.println(TEMPERATURE_SENSOR_GPIO, DEC);

  // STEP 1: scan for connected devices on the OneWire bus:
  uint8_t deviceCount = scanDevicesAddressesAndRememberLast(temperatureSensorBus, tempSensorDeviceAddress);
  if (deviceCount != 1) {
    ErrorMessages::DeviceCountError::build(ErrorMessages::errorBuffer, TEMPERATURE_SENSOR_GPIO, deviceCount);
    displayErrorAndHalt(String(ErrorMessages::getBuffer())); // will block indefinitely
  }

  // STEP 2: Pre-Init Sanity Checks that the device is supported by the DallasTemperature driver:
  Serial.print(F("Verifying that sensor at address "));
  printDeviceAddress(tempSensorDeviceAddress);
  Serial.println(F(" is supported by the DallasTemperature driver."));

  if (!(temperatureSensors.validAddress(tempSensorDeviceAddress))) { // confirm that the address is valid
    ErrorMessages::IncompatibleAddressError::build(ErrorMessages::errorBuffer, tempSensorDeviceAddress);
    displayErrorAndHalt(String(ErrorMessages::getBuffer())); // will block indefinitely
  }
  if (!(temperatureSensors.validFamily(tempSensorDeviceAddress))) { // confirm the device is supported by the driver
    ErrorMessages::UnknownDeviceError::build(ErrorMessages::errorBuffer, tempSensorDeviceAddress);
    displayErrorAndHalt(String(ErrorMessages::getBuffer())); // will block indefinitely
  }

  // STEP 3: Init Temperature Sensor
  temperatureSensors.begin();                     // Initialize the sensor.
  temperatureSensors.setWaitForConversion(false); // makes `sensors.requestTemperaturesByAddress` non-blocking, need to track time manually after requesting temperature conversion

  // STEP 4: Post-Init Sanity Checks that the DS18B20 temperature sensor is properly connected
  if (!(temperatureSensors.isConnected(tempSensorDeviceAddress))) { // sanity check: the device at the expected address is reported as connected
    ErrorMessages::SensorDisconnectedError::build(ErrorMessages::errorBuffer, tempSensorDeviceAddress);
    displayErrorAndHalt(String(ErrorMessages::getBuffer())); // will block indefinitely
  }

  // Check that sensor is not reporting parasite power mode. Parasite power mode is not expected and likely a symptom of some defect.
  if (temperatureSensors.readPowerSupply(tempSensorDeviceAddress)) { // Read device's power requirements. Return 1 if device needs parasite power.
    // Serial.print(F("WARNING: DS18B20 temperature sensor "));
    // printDeviceAddress(tempSensorDeviceAddress);
    // Serial.println(F(" is reporting PARASITE POWER MODE. This is unexpected and may indicate a defect."));
    ErrorMessages::ParasitePowerError::build(ErrorMessages::errorBuffer, tempSensorDeviceAddress);
    displayErrorAndHalt(String(ErrorMessages::getBuffer())); // will block indefinitely
  }

  // set the temperature accuracy
  // Note on `skipGlobalBitResolutionCalculation` parameter:
  // When skipGlobalBitResolutionCalculation is set to true, the function will only set the resolution for the targeted device and will not recalculate or update the overall (global) bit
  // resolution for all devices on the bus. This can be useful for performance reasons or when you want to manage device resolutions individually without affecting the global setting.
  // Conversely, if skipGlobalBitResolutionCalculation is false, the function will update the global bit resolution variable after successfully setting the device's resolution. It will also
  // scan all devices to ensure the global bit resolution reflects the highest resolution among all connected sensors. This ensures consistency when reading temperatures from multiple devices.
  temperatureSensors.setResolution(tempSensorDeviceAddress, TEMPERATURE_PRECISION);

  // verify resolution setting:
  uint8_t actualPrecision = temperatureSensors.getResolution(tempSensorDeviceAddress);
  if (actualPrecision != TEMPERATURE_PRECISION) {
    ErrorMessages::PrecisionSettingError::build(ErrorMessages::errorBuffer, tempSensorDeviceAddress, TEMPERATURE_PRECISION, actualPrecision);
    displayErrorAndHalt(String(ErrorMessages::getBuffer())); // will block indefinitely
  }
  Serial.print(F("Successfully set temperature sensor to precision of "));
  Serial.print(actualPrecision, DEC);
  Serial.println(F(" bits."));

  // Happy path: initial temperature read
  // • Request temperature measurement.
  // • Wait sufficient time for measurement to complete.
  // • As a sanity check, we measure and log the time taken for the initial temperature read. For normal operations
  //   we must ensure that the delay is short enough compared to the desired periodicity of the temperature read.
  //   As a sanity check, we require that the delay is less than 75% of the request interval.
  //   Given the delay `REQUEST_TEMP_INTERVAL_MS` (unsigned int value), 75% of this can be efficiently computed as
  //   `REQUEST_TEMP_INTERVAL_MS - (REQUEST_TEMP_INTERVAL_MS >> 2)` using bit shift for division by 4.
  // • Read temperature and log the current value to Serial console.
  temperatureSensors.requestTemperaturesByAddress(tempSensorDeviceAddress); // Request temperature conversion and wait for it to complete
  int64_t tstart = esp_timer_get_time();
  int64_t timeout = tstart + static_cast<int64_t>(REQUEST_TEMP_INTERVAL_MS - (REQUEST_TEMP_INTERVAL_MS >> 2)) * 1000LL;
  retrieveTemp.activate(TEMP_READ_DELAY_MS); // Wait for temperature read to complete (at 10-bit this should be ~187.5ms)
  while (true) {
    int64_t t = esp_timer_get_time();
    if (retrieveTemp.checkTrigger(t)) break; // updated temperature ready for retrieval
    if (timeout < t) {                       // waiting for temperature read takes too long for stable operations!
      ErrorMessages::TemperatureDelayError::build(ErrorMessages::errorBuffer, TEMP_READ_DELAY_MS);
      displayErrorAndHalt(String(ErrorMessages::getBuffer())); // will block indefinitely
    }
    delay(3);
  }
  int64_t delta = esp_timer_get_time() - tstart; // delay in MICROseconds
  Serial.print(F("Operating with latency of "));
  Serial.print(delta / 1000LL); // convert delay to milliseconds
  Serial.println(F("ms between requesting and retrieving temperature measurement."));

  float tempC = readTemp(temperatureSensors, tempSensorDeviceAddress); // in case of error: blocks and displays error display indefinitely
  Serial.print(F("Current temperature: "));
  Serial.print(tempC);
  Serial.print(F(" C / "));
  Serial.print(DallasTemperature::toFahrenheit(tempC));
  Serial.println(F(" F"));

  // TODO: for debuging and testing, to be removed
  // while (true) {
  //   Serial.print(F(" requesting temp ..."));
  //   temperatureSensors.requestTemperaturesByAddress(tempSensorDeviceAddress); // Request temperature conversion and wait for it to complete
  //   retrieveTemp.activate(TEMP_READ_DELAY_MS);

  //   while (!retrieveTemp.checkTrigger()) { // Wait for temperature read to complete (at 10-bit: ~187.5ms)
  //     delay(3);
  //   }
  //   Serial.print(F(" reading temp: "));
  //   float tempC = readTemp(temperatureSensors, tempSensorDeviceAddress);
  //   printTemperature(tempC, true);
  //   Serial.println();

  //   delay(5000);
  // }

  Serial.println(F("DS18B20 temperature sensor successfully initialized\n"));
  return tempC;
}

// printTemperature prints the temperature to Serial console in units of Celsius and optionally Fahrenheit
// (off by default). In case of error (e.g., sensor disconnected), an error message is printed instead.
// CAUTION: does not print new line at the end.
void printTemperature(float tempC, bool printFahrenheit /* = false */) {
  if (!std::isfinite(tempC)) {
    Serial.println();
    Serial.print(F("ERROR: temperature measurement failed with value "));
    Serial.println(tempC);
    return;
  }

  Serial.print(tempC);
  Serial.print(F(" C"));
  if (printFahrenheit) {
    Serial.print(F("  / "));
    Serial.print(DallasTemperature::toFahrenheit(tempC));
    Serial.print(F(" F"));
  }
}

// readTemp retrieves the temperature and returns the temperature in Celsius as float.
// We apply basic sanity checks on the read temperature value:
//  • the read value is not DEVICE_DISCONNECTED_C (indicating sensor disconnected)
//    as defined by the DallasTemperature library
//  • the read value is in the closed interval [ TEMP_SENSOR_LOEWEST_VALID , TEMP_SENSOR_LARGEST_VALID ]
//
// ATTENTIONK: when a retrieved temp value is outside the valid range, we print an ERROR message to
// Serial console and OLED display, and this function BLOCKs INDEFINITELY.
//
// REQUIREMENT:
// This function expects that `waitForConversion` is set to false, i.e. temperature measurements are non-blocking.
// Specifically, they must be requested via `requestTemperaturesByAddress` and need sufficient time to complete
// before calling this function.
float readTemp(DallasTemperature &sensors, DeviceAddress deviceAddress) {
  float tempC = sensors.getTempC(deviceAddress);

  // Sanity check that temperature read is within valid range:
  if (tempC == DEVICE_DISCONNECTED_C) tempC = NAN;
  if (tempC < TEMP_SENSOR_LOEWEST_VALID) tempC = -INFINITY;
  if (tempC > TEMP_SENSOR_LARGEST_VALID) tempC = INFINITY;
  if (!std::isfinite(tempC)) {
    ErrorMessages::TemperatureReadError::build(ErrorMessages::errorBuffer, tempC);
    displayErrorAndHalt(String(ErrorMessages::getBuffer())); // will block indefinitely
  }

  return tempC;
}

/* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Notes ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌

• example for displaying temperature on web server hosted by the MCU: https://randomnerdtutorials.com/esp32-ds18b20-temperature-arduino-ide/



*/

// TODO
// CAUTION: C++ does not require that an implementation supports infinity. However, the std::numeric_limits<T>::is_iec559 check can confirm IEEE 754 compliance, which is standard on most modern platforms.
// std::numeric_limits<T>::is_iec559
