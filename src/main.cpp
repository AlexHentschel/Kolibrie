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

U8G2_SSD1306_72X40_ER_F_SW_I2C u8g2(U8G2_R2, 6, 5, U8X8_PIN_NONE);
// U8G2_R0 	No rotation, landscape
// U8G2_R1 90 degree clockwise rotation
// U8G2_R2 180 degree clockwise rotation
// U8G2_R3 270 degree clockwise rotation

/* DS18B20 Temperature Sensor
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
#define TEMPERATURE_SENSOR_GPIO 2 // DS18B20 is connected to GPIO 2; this is the port for the OneWire bus
#define TEMPERATURE_PRECISION 10  // select 10 bit precision for DS18B20 (available range is 9 to 12 bits): corresponds to 0.25°C resolution with 187.5 ms measurement duration
OneWire temperatureSensorBus(TEMPERATURE_SENSOR_GPIO);
DallasTemperature temperatureSensors(&temperatureSensorBus);

std::unique_ptr<FrequencyTrigger> triggerReadTemp = nullptr;

/* LEDs
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
#define BLUE_LED_BUILTIN 8 // GPIO 8, Blue LED: LOW = on, HIGH = off

// LEDs' blinking to indicate that temperature was measured successfully
LEDExpiringToggler *tempMeasurementSuccess = nullptr; // blinks 5 times turning 1 second

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

/* IO and APIs
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
// prints life-signs to Serial console, unbounded runtime, print every 7331 milliseconds.
// Notes:
//  * Static allocation avoids memory leak; object persists for application lifetime
//  * We choose an larger prime interval to avoid accidental synchronization with other periodic tasks
static PrintLifeSign consolePrintLifeSignInstance(FrequencyUtils::unbounded_lifetime, 7331, "Controller alive");
PrintLifeSign *consolePrintLifeSign = &consolePrintLifeSignInstance;

std::unique_ptr<StatDisplay> statDisplay = nullptr;

std::unique_ptr<DisplayScrollText> oledLine1Scroller = nullptr;
std::unique_ptr<DisplayScrollText> oledLine1Scroller2 = nullptr;

std::unique_ptr<ErrDisplay> errDisplay = nullptr;

int64_t startMicros = 0;
int testStateCounter = 0;

/* ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ CONTROLLER INITIALIZATION ▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅▅ */

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ ERROR MESSAGE FRAMEWORK ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
// This framework provides compile-time validated, heap-fragmentation-free error messages. Each error
// message is defined once with its string components and runtime build logic. Compile-time validation
// using worst-case values for the constituent components ensures all messages fit within the static buffer.
//   We heavily use the `constexpr` keyword. This tells the compiler this function *can* be evaluated at
// compile time. Specifically, when *all inputs* are known at compile time, the compiler executes the function
// during compilation.

namespace ErrorMessages {
  constexpr size_t BUFFER_SIZE = 150;

  // const_strlen: helper function to calculate the worst-case string length at compile time.
  // The `constexpr` keyword tells the compiler this function *can* be evaluated at compile time:
  // specifically, when *all inputs* are known at compile time, the compiler executes the function
  // during compilation.
  constexpr size_t const_strlen(const char *str) {
    // Iterates through string until null terminator is found, counting characters:
    size_t len = 0;
    while (str[len] != '\0') {
      len++;
    }
    return len;
  }

  // intLength: helper function to calculate the number of character required to display an integer
  // in decimal representation. This function supports evaluation at compile time, which is useful
  // for determining the worst-case length of error messages.
  //   The `constexpr` keyword tells the compiler this function *can* be evaluated at compile time:
  // specifically, when *all inputs* are known at compile time, the compiler executes the function
  // during compilation.
  //
  // FUTURE-PROOFING: Uses the platform's native int type and its corresponding unsigned type.
  // Works correctly on both 32-bit and 64-bit platforms.
  constexpr size_t intLength(int value) {
    // Counts digits by repeatedly dividing by 10 until value becomes 0
    // Accounts for minus sign by adding 1 to length for negative values
    // Example: intLength(255) = 3, intLength(-99) = 3 (includes minus sign)
    if (value == 0) return 1;

    // CAUTION with smallest possible negative value. This is because the smallest negative value INT_MIN
    // (e.g., -2147483648 for 32-bit, -9223372036854775808 for 64-bit) has an absolute value one larger
    // than INT_MAX. Hence, we cannot simply invert the sign without risk of overflow. Instead, we count
    // the minus sign consuming one character and then convert to the unsigned type to safely represent
    // the absolute value.
    size_t len = 0;
    unsigned int absValue;  // matches int's size on the platform
    if (value < 0) {
      len = 1;                                                // Count minus sign
      absValue = static_cast<unsigned int>(-(value + 1)) + 1; // Avoid overflow
    } else {
      absValue = static_cast<unsigned int>(value);
    }
    // at this point, the uncounted digits of absValue represent a positive number, i.e. at least 1 digit:

    do { // Count digits by repeatedly dividing by 10 until absValue becomes 0
      // since integer divisions are much slower than comparisons, and numbers are typically small, we explicitly
      // check for single-digit numbers, then double-digit numbers, and lastly triple-digit numbers:
      if (absValue <= 9) return len + 1;
      if (absValue <= 99) return len + 2;
      if (absValue <= 999) return len + 3;

      len += 3;
      absValue /= 1000;
    } while (absValue > 0);
    return len;
  }

  // Calculate maximum buffer size needed for int-to-string conversion at compile time.
  // We use exact values derived from the mathematical maximum for each integer width:
  //   - 16-bit signed int: -32768 → 6 chars (5 digits + sign + null = 7 bytes)
  //   - 32-bit signed int: -2147483648 → 11 chars (10 digits + sign + null = 12 bytes)
  //   - 64-bit signed int: -9223372036854775808 → 20 chars (19 digits + sign + null = 21 bytes)
  //
  // Note: We cannot use log10() as it's not constexpr until C++26. Instead, we use the
  // mathematically derived digit count for INT_MIN at each common integer width.
  //
  // The fallback uses a heuristic approximation based on:
  //   Formula: ceil(log₁₀(2**bits)) + 2 = ceil(bits * log₁₀(2)) + 2 = ceil(bits × 0.30103) + 2
  //   Simplified to: bits / 3 + 3 (slightly conservative, adds 1-2 extra bytes)
  //   This accounts for: digits + sign + null terminator
  constexpr size_t INT_STRING_BUFFER_SIZE = 
    sizeof(int) == 2 ? 7 :   // 16-bit: "-32768\0"
    sizeof(int) == 4 ? 12 :  // 32-bit: "-2147483648\0"
    sizeof(int) == 8 ? 21 :  // 64-bit: "-9223372036854775808\0"
    sizeof(int) * 8 / 3 + 3; // Fallback approximation for exotic platforms

  /* ━━━━━━━━━━━━━━━━━━━━━━━━ RUNTIME BUFFER HELPERS ━━━━━━━━━━━━━━━━━━━━━━━━ */

  // Write string literal to buffer at current position, advance position
  inline void writeString(char *buffer, size_t &pos, const char *str) {
    while (*str != '\0' && pos < BUFFER_SIZE - 1) {
      buffer[pos++] = *str++;
    }
  }

  // Write integer to buffer at current position, advance position
  // Uses Arduino's itoa() function which handles all edge cases including INT_MIN.
  // itoa() uses the same division-based algorithm under the hood, but is battle-tested and maintained.
  //
  // FUTURE-PROOFING: Buffer size is calculated at compile time based on platform's int size.
  // Works correctly on both 32-bit (12 bytes) and 64-bit (22 bytes) platforms.
  inline void writeInt(char *buffer, size_t &pos, int value) {
    char temp[INT_STRING_BUFFER_SIZE]; // stack-allocated buffer
    itoa(value, temp, 10);  // Convert to base-10 decimal string
    
    // Copy result to buffer
    const char *p = temp;
    while (*p != '\0' && pos < BUFFER_SIZE - 1) {
      buffer[pos++] = *p++;
    }
  }

  // Write float to buffer at current position (2 decimal places), advance position
  // 
  // CAUTION: Casting float to int is only safe when the float value is within int's range.
  // For 32-bit int: -2,147,483,648 to 2,147,483,647
  // For 64-bit int: -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807
  // Casting an out-of-range float to int is undefined behavior. We handle overflow by
  // displaying a special message.
  inline void writeFloat(char *buffer, size_t &pos, float value) {
    // Handle special cases
    if (isnan(value)) {
      writeString(buffer, pos, "nan");
      return;
    }
    if (isinf(value)) {
      writeString(buffer, pos, value < 0 ? "-inf" : "inf");
      return;
    }

    // Handle negative
    bool isNegative = (value < 0);
    if (isNegative) {
      buffer[pos++] = '-';
      value *= -1;
    }

    // Check if float value fits in int range before casting
    // Note: INT_MAX is typically 2147483647 (32-bit) or 9223372036854775807 (64-bit)
    constexpr float INT_MAX_FLOAT = static_cast<float>(INT_MAX);
    if (value > INT_MAX_FLOAT) {
      writeString(buffer, pos, "overflow");
      return;
    }

    // Integer part (now safe to cast)
    int intPart = static_cast<int>(value);
    writeInt(buffer, pos, intPart);

    // Decimal point
    buffer[pos++] = '.';

    // Two decimal places
    int decimal = static_cast<int>((value - intPart) * 100.0f + 0.5f); // the addition of 0.5f is to round the value to the nearest integer
    if (decimal < 10) buffer[pos++] = '0';
    writeInt(buffer, pos, decimal);
  }

  // Calculate the string length of a formatted DeviceAddress at compile time.
  // DeviceAddress is typedef uint8_t DeviceAddress[8], formatted as "XX.XX.XX.XX.XX.XX.XX.XX"
  // Each byte takes 2 hex characters (with leading zero if needed), plus dots between bytes.
  constexpr size_t deviceAddressStringLength() {
    constexpr size_t numBytes = sizeof(DeviceAddress); // Automatically derived from typedef
    constexpr size_t hexCharsPerByte = 2;              // Always 2 hex digits (e.g., "0F" or "A3")
    constexpr size_t numSeparators = numBytes - 1;     // Dots between bytes
    return numBytes * hexCharsPerByte + numSeparators; // 8 * 2 + 7 = 23
  }

  // Helper function for compile-time max
  constexpr size_t constexpr_max(size_t a, size_t b) {
    return (a > b) ? a : b;
  }

  // Calculate the maximum string length that writeFloat can produce at compile time.
  // This covers ALL possible outputs from writeFloat, including malfunction cases.
  constexpr size_t maxFloatStringLength() {
    // Special cases that writeFloat can produce:
    constexpr size_t nanLength = 3;           // "nan"
    constexpr size_t infLength = 4;           // "-inf" (worst case with sign)
    constexpr size_t overflowLength = 9;      // "-overflow" (sign + "overflow")
    
    // Normal number format: optional '-' + integer part + '.' + 2 decimal digits
    // Worst case: "-" + INT_MAX digits + "." + "99"
    constexpr size_t normalLength = 1 + intLength(INT_MAX) + 1 + 2; // sign + int + dot + decimals
    
    // Return the maximum of all possible cases
    // For 32-bit int: max(3, 4, 9, 14) = 14
    // For 64-bit int: max(3, 4, 9, 23) = 23
    return constexpr_max(constexpr_max(nanLength, infLength), 
                         constexpr_max(overflowLength, normalLength));
  }

  // Write device address to buffer at current position, advance position
  // Convert a OneWire device address to a String in hexadecimal format
  // Uses input buffer directly, no heap allocation.
  inline void writeDeviceAddress(char *buffer, size_t &pos, const DeviceAddress address) {
    // DeviceAddress is defined as `typedef uint8_t DeviceAddress[8];`
    // Loop count is automatically derived from the actual type size
    constexpr size_t numBytes = sizeof(DeviceAddress); // determined at compile time
    
    for (uint8_t i = 0; i < numBytes; i++) {
      // Leading zero for values < 0x10
      if (address[i] < 0x10) {
        buffer[pos++] = '0';
      }
      // High nibble
      uint8_t highNibble = (address[i] >> 4) & 0x0F;
      buffer[pos++] = (highNibble < 10) ? ('0' + highNibble) : ('A' + highNibble - 10); // addition is character arithmetic, e.g. A+1 = B
      // Low nibble
      uint8_t lowNibble = address[i] & 0x0F;
      buffer[pos++] = (lowNibble < 10) ? ('0' + lowNibble) : ('A' + lowNibble - 10); // addition is character arithmetic

      // Dot separator (except after last byte)
      if (i < numBytes - 1) {
        buffer[pos++] = '.';
      }
    }
  }

  /* ━━━━━━━━━━━━━━━━━━━━━━━━ ERROR MESSAGE DEFINITIONS ━━━━━━━━━━━━━━━━━━━━━━━━ */

  // Error 1: Device count mismatch
  struct DeviceCountError {
    static constexpr const char *prefix = " Expected 1 device on GPIO ";
    static constexpr const char *middle = ", found ";

    static constexpr size_t worstCaseLength(uint8_t maxGpio = 255, uint8_t maxDevices = 255) {
      return const_strlen(prefix) + intLength(static_cast<int>(maxGpio)) +
             const_strlen(middle) + intLength(static_cast<int>(maxDevices));
    }

    static void build(char *buffer, uint8_t gpio, uint8_t deviceCount) {
      size_t pos = 0;
      writeString(buffer, pos, prefix);
      writeInt(buffer, pos, static_cast<int>(gpio));
      writeString(buffer, pos, middle);
      writeInt(buffer, pos, static_cast<int>(deviceCount));
      buffer[pos] = '\0';
    }
  };

  // Error 2: Incompatible device address
  struct IncompatibleAddressError {
    static constexpr const char *prefix = " Incompatible device address ";

    static constexpr size_t worstCaseLength() {
      return const_strlen(prefix) + deviceAddressStringLength();
    }

    static void build(char *buffer, const DeviceAddress addr) {
      size_t pos = 0;
      writeString(buffer, pos, prefix);
      writeDeviceAddress(buffer, pos, addr);
      buffer[pos] = '\0';
    }
  };

  // Error 3: Unknown device type
  struct UnknownDeviceError {
    static constexpr const char *prefix = " Unknown device type at address ";

    static constexpr size_t worstCaseLength() {
      return const_strlen(prefix) + deviceAddressStringLength();
    }

    static void build(char *buffer, const DeviceAddress addr) {
      size_t pos = 0;
      writeString(buffer, pos, prefix);
      writeDeviceAddress(buffer, pos, addr);
      buffer[pos] = '\0';
    }
  };

  // Error 4: Sensor disconnected after init
  struct SensorDisconnectedError {
    static constexpr const char *prefix = " DS18B20 Temp Sensor ";
    static constexpr const char *suffix = " still disconnected after initialization";

    static constexpr size_t worstCaseLength() {
      return const_strlen(prefix) + deviceAddressStringLength() + const_strlen(suffix);
    }

    static void build(char *buffer, const DeviceAddress addr) {
      size_t pos = 0;
      writeString(buffer, pos, prefix);
      writeDeviceAddress(buffer, pos, addr);
      writeString(buffer, pos, suffix);
      buffer[pos] = '\0';
    }
  };

  // Error 5: Parasite power mode
  struct ParasitePowerError {
    static constexpr const char *prefix = " DS18B20 Temp Sensor ";
    static constexpr const char *suffix = " is reporting parasite power mode";

    static constexpr size_t worstCaseLength() {
      return const_strlen(prefix) + deviceAddressStringLength() + const_strlen(suffix);
    }

    static void build(char *buffer, const DeviceAddress addr) {
      size_t pos = 0;
      writeString(buffer, pos, prefix);
      writeDeviceAddress(buffer, pos, addr);
      writeString(buffer, pos, suffix);
      buffer[pos] = '\0';
    }
  };

  // Error 6: Precision setting failed
  struct PrecisionSettingError {
    static constexpr const char *part1 = " Setting precision for DS18B20 Temp Sensor ";
    static constexpr const char *part2 = " to ";
    static constexpr const char *part3 = " bits failed; sensor still reports ";
    static constexpr const char *part4 = " bits";

    static constexpr size_t worstCaseLength(uint8_t maxPrecision = 12) {
      return const_strlen(part1) + deviceAddressStringLength() + const_strlen(part2) +
             intLength(static_cast<int>(maxPrecision)) + const_strlen(part3) +
             intLength(static_cast<int>(maxPrecision)) + const_strlen(part4);
    }

    static void build(char *buffer, const DeviceAddress addr, uint8_t desired, uint8_t actual) {
      size_t pos = 0;
      writeString(buffer, pos, part1);
      writeDeviceAddress(buffer, pos, addr);
      writeString(buffer, pos, part2);
      writeInt(buffer, pos, static_cast<int>(desired));
      writeString(buffer, pos, part3);
      writeInt(buffer, pos, static_cast<int>(actual));
      writeString(buffer, pos, part4);
      buffer[pos] = '\0';
    }
  };

  // Error 7: Temperature read failed
  struct TemperatureReadError {
    static constexpr const char *prefix = " Temperature read failed with value ";

    static constexpr size_t worstCaseLength() {
      // Use the maximum length that writeFloat can produce, which handles all possible
      // float values including special cases (nan, inf, overflow) and malfunction scenarios.
      // For 32-bit int: maxFloatStringLength() = 14 (e.g., "-2147483647.99")
      // For 64-bit int: maxFloatStringLength() = 23
      return const_strlen(prefix) + maxFloatStringLength();
    }

    static void build(char *buffer, float value) {
      size_t pos = 0;
      writeString(buffer, pos, prefix);
      writeFloat(buffer, pos, value);
      buffer[pos] = '\0';
    }
  };

  /* ━━━━━━━━━━━━━━━━━━━━━ COMPILE-TIME VALIDATION ━━━━━━━━━━━━━━━━━━━━━━━━ */

  static_assert(DeviceCountError::worstCaseLength() < BUFFER_SIZE,
                "DeviceCountError exceeds buffer size!");

  static_assert(IncompatibleAddressError::worstCaseLength() < BUFFER_SIZE,
                "IncompatibleAddressError exceeds buffer size!");

  static_assert(UnknownDeviceError::worstCaseLength() < BUFFER_SIZE,
                "UnknownDeviceError exceeds buffer size!");

  static_assert(SensorDisconnectedError::worstCaseLength() < BUFFER_SIZE,
                "SensorDisconnectedError exceeds buffer size!");

  static_assert(ParasitePowerError::worstCaseLength() < BUFFER_SIZE,
                "ParasitePowerError exceeds buffer size!");

  static_assert(PrecisionSettingError::worstCaseLength(12) < BUFFER_SIZE,
                "PrecisionSettingError exceeds buffer size!");

  static_assert(TemperatureReadError::worstCaseLength() < BUFFER_SIZE,
                "TemperatureReadError exceeds buffer size!");

  /* ━━━━━━━━━━━━━━━━━━━━ RUNTIME BUFFER MANAGEMENT ━━━━━━━━━━━━━━━━━━━━━━ */

  static char errorBuffer[BUFFER_SIZE];

  const char *getBuffer() {
    return errorBuffer;
  }
}

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

void setup() { /* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
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
  tempMeasurementSuccess = new LEDExpiringToggler(BLUE_LED_BUILTIN, 1200, 500, 200, LedUtils::LOW_IS_ON);

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
  triggerReadTemp = std::make_unique<FrequencyTrigger>(FrequencyUtils::unbounded_lifetime, 5000u); // read temperature every 5s, unbounded lifetime

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Happy Path Status Display ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */

  statDisplay = std::make_unique<StatDisplay>(u8g2, 700, 300);
  statDisplay->setHeatingStatus(false);
  statDisplay->setWifiStatus(false);
  statDisplay->setTemp(tempC);

  /* ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ start ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ */
  consolePrintLifeSign->activate(293);

  triggerReadTemp->activate();

  tempMeasurementSuccess->activate();
  extLoadToggler->activate();

  triggerReadTemp->activate(421);

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
// providing a space betwen the line leaving the display and the repeated message scrolling into the display.
void displayErrorAndHalt(const String &errorMessage) {
  // Print to Serial console
  Serial.println();
  Serial.print(F("ERROR:")); // leading blank of `errorMessage` is provided by the caller
  Serial.println(errorMessage);
  Serial.println(F("Halting execution."));

  // Create and activate error display
  DisplayText headline = DisplayText(u8g2, String(F(" ERROR")), 20);
  DisplayText detailedMsg = DisplayText(u8g2, errorMessage, 16);
  errDisplay = std::make_unique<ErrDisplay>(u8g2, headline, detailedMsg, 20);

  // Infinite loop: keep updating the error display
  while (true) {
    int64_t currentMicros = esp_timer_get_time();
    errDisplay->checkRedraw(currentMicros);
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
// (which Serial inherits from) is specifically designed to avoid dynamic allocation for primtive
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
  temperatureSensors.begin(); // Initialise the sensor.

  // STEP 4: Post-Init Sanity Checks that the DS18B20 temperature sensor is properly connected
  if (!(temperatureSensors.isConnected(tempSensorDeviceAddress))) { // sanity check: the device at the expected address is reported as connected
    ErrorMessages::SensorDisconnectedError::build(ErrorMessages::errorBuffer, tempSensorDeviceAddress);
    displayErrorAndHalt(String(ErrorMessages::getBuffer()));
  }

  // Check that sensor is not reporting parasite power mode, which would not be expected and likely a symptom of some defect
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
  return tempC;
}

// printTemperature
// • measured the temperature,
// • prints the temperature to Serial console in units of Celsius and Fahrenheit
// • in case of error (e.g., sensor disconnected), an error message is printed instead
// • returns the temperature in Celsius as float; in case of error, NaN is returned
float printTemperature(DallasTemperature &sensors, DeviceAddress deviceAddress) {
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
  float tempC = sensors.getTempC(deviceAddress);
  if ((tempC == DEVICE_DISCONNECTED_C) {
    return NAN;
  }
  return tempC;
}
