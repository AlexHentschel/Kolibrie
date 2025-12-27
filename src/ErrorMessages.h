#pragma once

// Required dependencies
#include "DallasTemperature.h" // For DeviceAddress typedef
#include <Arduino.h>           // For itoa(), isnan(), isinf()
#include <cstddef>             // For size_t
#include <cstdint>             // For uint8_t, INT_MAX

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ ERROR MESSAGE FRAMEWORK ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
// This framework provides compile-time validated, heap-fragmentation-free error messages. Each error
// message is defined once with its string components and runtime build logic. Compile-time validation
// using worst-case values for the constituent components ensures all messages fit within the static buffer.
//   We heavily use the `constexpr` keyword. This tells the compiler this function *can* be evaluated at
// compile time. Specifically, when *all inputs* are known at compile time, the compiler executes the function
// during compilation.
//
// THREAD SAFETY: This framework is designed for SINGLE-THREADED use only. The static errorBuffer is not
// thread-safe. If you need to use this in a multi-threaded environment (e.g., FreeRTOS), you must add
// appropriate synchronization (mutex/semaphore) before calling any build() functions.
//
// TRUNCATION HANDLING: If a message would exceed the buffer size, it will be truncated and the special
// character '»' (ASCII 187) will be appended as the last character before the null terminator to indicate
// truncation occurred.

namespace ErrorMessages {
  constexpr size_t BUFFER_SIZE = 180;
  constexpr char TRUNCATION_MARKER = static_cast<char>(187); // '»' character to indicate truncation in U8G2 default fonts, such as `u8g2_font_7x13_tf`

  // External declaration of the error buffer (defined in ErrorMessages.cpp)
  extern char errorBuffer[BUFFER_SIZE];

  // Get pointer to the error buffer
  const char *getBuffer();

  /* ━━━━━━━━━━━━━━━━━━━━ COMPILE-TIME LENGTH CALCULATION HELPERS ━━━━━━━━━━━━━━━━━━━━━━━━ */

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
    unsigned int absValue; // matches int's size on the platform
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
      sizeof(int) == 2 ? 7 : // 16-bit: "-32768\0"
          sizeof(int) == 4 ? 12
                           : // 32-bit: "-2147483648\0"
          sizeof(int) == 8 ? 21
                           :       // 64-bit: "-9223372036854775808\0"
          sizeof(int) * 8 / 3 + 3; // Fallback approximation for exotic platforms

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
    constexpr size_t nanLength = 3;      // "nan"
    constexpr size_t infLength = 4;      // "-inf" (worst case with sign)
    constexpr size_t overflowLength = 9; // "-overflow" (sign + "overflow")

    // Normal number format: optional '-' + integer part + '.' + 2 decimal digits
    // Worst case: "-" + INT_MAX digits + "." + "99"
    constexpr size_t normalLength = 1 + intLength(INT_MAX) + 1 + 2; // sign + int + dot + decimals

    // Return the maximum of all possible cases
    // For 32-bit int: max(3, 4, 9, 14) = 14
    // For 64-bit int: max(3, 4, 9, 23) = 23
    return constexpr_max(constexpr_max(nanLength, infLength),
                         constexpr_max(overflowLength, normalLength));
  }

  /* ━━━━━━━━━━━━━━━━━━━━━━━━ RUNTIME BUFFER HELPERS ━━━━━━━━━━━━━━━━━━━━━━━━ */
  // All write functions respect buffer boundaries and will truncate gracefully if needed.
  // After truncation, the TRUNCATION_MARKER character is added before the null terminator.

  // Helper to check if we have space for at least n more characters (happy path)
  // This assumes successful write with only null terminator needed at the end.
  // Individual write functions handle truncation internally if space runs out.
  inline bool hasSpace(size_t pos, size_t needed) {
    // Happy path: current pos + needed chars + null terminator
    return (pos + needed + 1 <= BUFFER_SIZE);
  }

  // Helper to mark truncation at current position, where `pos` marks the next empty
  // position in the buffer.
  // Strategy:
  //   1. If there's space at current position for marker + null, add them there
  //   2. Otherwise, overwrite the last two buffer positions with marker + null
  inline void markTruncation(char *buffer, size_t &pos) {
    if (pos <= BUFFER_SIZE - 2) {
      // Space available: add truncation marker at current position, then null
      buffer[pos++] = TRUNCATION_MARKER;
      buffer[pos] = '\0';
    } else {
      // Not enough space remaining in buffer to add truncation marker and null terminator. Hence, we
      // overwrite the last two positions of the buffer with the truncation marker and null terminator.
      buffer[BUFFER_SIZE - 2] = TRUNCATION_MARKER;
      buffer[BUFFER_SIZE - 1] = '\0';
      pos = BUFFER_SIZE - 1;
    }
  }

  // Write single character to buffer at current position, advance position
  // Returns true if fully written, false if truncated
  inline bool writeChar(char *buffer, size_t &pos, char c) {
    if (!hasSpace(pos, 1)) {
      markTruncation(buffer, pos);
      return false;
    }
    buffer[pos++] = c;
    return true;
  }

  // Write string literal to buffer at current position, advance position
  // Returns true if fully written, false if truncated
  inline bool writeString(char *buffer, size_t &pos, const char *str) {
    while (*str != '\0') {
      if (!writeChar(buffer, pos, *str)) {
        return false; // writeChar already handled truncation
      }
      str++;
    }
    return true;
  }

  // Write integer to buffer at current position, advance position
  // Uses Arduino's itoa() function which handles all edge cases including INT_MIN.
  // Returns true if fully written, false if truncated
  //
  // FUTURE-PROOFING: Buffer size is calculated at compile time based on platform's int size.
  // Works correctly on both 32-bit (12 bytes) and 64-bit (22 bytes) platforms.
  inline bool writeInt(char *buffer, size_t &pos, int value) {
    char temp[INT_STRING_BUFFER_SIZE]; // stack-allocated buffer
    itoa(value, temp, 10);             // Convert to base-10 decimal string

    // Copy result to buffer
    // Happy path: write while we have space for at least 1 char + null terminator
    const char *p = temp;
    while (*p != '\0' && hasSpace(pos, 1)) {
      buffer[pos++] = *p++;
    }

    if (*p != '\0') { // Check if we truncated, and insert truncation marker if needed
      markTruncation(buffer, pos);
      return false;
    }
    return true;
  }

  // Write float to buffer at current position (2 decimal places), advance position
  // Returns true if fully written, false if truncated
  //
  // CAUTION: Casting float to int is only safe when the float value is within int's range.
  // For 32-bit int: -2,147,483,648 to 2,147,483,647
  // For 64-bit int: -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807
  // Casting an out-of-range float to int is undefined behavior. We handle overflow by
  // displaying a special message.
  inline bool writeFloat(char *buffer, size_t &pos, float value) {
    // Special cases: "nan", "-inf", "inf". No heap allocation occurs, because:
    // Those are string literals and hence stored in read-only memory (flash/PROGMEM).
    // The ternary operator just returns a pointer to one of them.
    if (isnan(value)) return writeString(buffer, pos, "nan");
    if (isinf(value)) return writeString(buffer, pos, value < 0 ? "-inf" : "inf");

    // Handle negative values: need space for the minus sign
    // NOTE: Negative zero (-0.0f) is treated as positive zero and formatted as "0.00"
    // since the condition (value < 0) is false for -0.0f per IEEE 754 standard.
    if (value < 0) {
      if (!hasSpace(pos, 1)) {
        markTruncation(buffer, pos);
        return false;
      }
      buffer[pos++] = '-';
      value = -value; // Better than `value *= -1` for precision
    }

    // Check if float value fits in int range before casting
    //
    // FLOAT PRECISION EDGE CASE: Due to float's limited precision (24-bit mantissa), INT_MAX
    // cannot be exactly represented in a float. For 32-bit int:
    //   - INT_MAX = 2,147,483,647 (requires 31 bits)
    //   - static_cast<float>(INT_MAX) ≈ 2,147,483,648.0 (rounds up to 2^31)
    // At this magnitude, consecutive representable floats are spaced 128 apart. This means
    // static_cast<float>(INT_MAX) actually produces a value GREATER than INT_MAX.
    // Therefore, we must use '>=' (not '>') to catch boundary cases where the float equals
    // the rounded INT_MAX, which would cause undefined behavior when cast to int.
    //
    // Note: INT_MAX is typically 2147483647 (32-bit) or 9223372036854775807 (64-bit)
    constexpr float INT_MAX_FLOAT = static_cast<float>(INT_MAX);
    if (value >= INT_MAX_FLOAT) {
      return writeString(buffer, pos, "overflow");
    }

    // Calculate decimal part first to handle rounding carry-over. Rounding might increase the integer part by 1,
    // so we must check before writing. Hypothetically, if we reversed the order by writing the integer part first,
    // and then rounded the fractional part, would introduce a bug where 1.996 would become "1.100" instead of "2.00".
    // This is because rounding might increase the integer part by 1, which would be incorrectly written to the buffer
    // if we wrote the integer part first.
    float fractional = value - static_cast<int>(value);
    int decimal = static_cast<int>(fractional * 100.0f + 0.5f); // two decimal places; rounded to nearest integer, and multiplied by 100
    int intPart = static_cast<int>(value);

    // Handle carry-over from rounding (e.g., 1.996 -> fractional=0.996 -> decimal=100)
    // Must check for overflow before incrementing to avoid wrapping past INT_MAX
    if (decimal >= 100) {
      if (intPart == INT_MAX) {
        return writeString(buffer, pos, "overflow");
      }
      intPart++;
      decimal = 0;
    }

    if (!writeInt(buffer, pos, intPart)) return false; // Write integer part
    if (!hasSpace(pos, 1)) {                           // Add decimal point provided there is space
      markTruncation(buffer, pos);
      return false;
    }
    buffer[pos++] = '.';

    // Two decimal places (now guaranteed decimal < 100)
    if (decimal < 10) {
      if (!hasSpace(pos, 1)) {
        markTruncation(buffer, pos);
        return false;
      }
      buffer[pos++] = '0';
    }

    return writeInt(buffer, pos, decimal);
  }

  // Write device address to buffer at current position, advance position
  // Convert a OneWire device address to a String in hexadecimal format
  // Uses input buffer directly, no heap allocation.
  // Returns true if fully written, false if truncated
  inline bool writeDeviceAddress(char *buffer, size_t &pos, const DeviceAddress address) {
    // DeviceAddress is defined as `typedef uint8_t DeviceAddress[8];`
    // Loop count is automatically derived from the actual type size
    constexpr size_t numBytes = sizeof(DeviceAddress); // determined at compile time

    for (uint8_t i = 0; i < numBytes; i++) {
      // Check if we have space for this byte (2 hex chars) + potential dot
      size_t neededSpace = 2 + (i < numBytes - 1 ? 1 : 0); // 2 hex chars, maybe 1 dot
      if (!hasSpace(pos, neededSpace)) {
        markTruncation(buffer, pos);
        return false;
      }

      // High nibble (automatically produces leading zero for values < 0x10)
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
    return true;
  }

  /* ━━━━━━━━━━━━━━━━━━━━━━━━ ERROR MESSAGE DEFINITIONS ━━━━━━━━━━━━━━━━━━━━━━━━ */
  //
  // NOTE: String literals in constexpr declarations are automatically stored in flash memory
  // (PROGMEM) on Arduino/ESP32 platforms and only loaded into RAM when accessed. We cannot
  // use the `F(…)` macro here because it's incompatible with constexpr:
  //   `F(…)` returns a special `__FlashStringHelper*` type at runtime, not a compile-time const `char*`
  // The compiler handles flash storage automatically for these string constants.

  // Error 1: Device count mismatch
  struct DeviceCountError {
    static constexpr const char *prefix = " Expected 1 device on GPIO "; // Stored in flash
    static constexpr const char *middle = ", found ";                    // Stored in flash

    static constexpr size_t worstCaseLength(uint8_t maxGpio = 255, uint8_t maxDevices = 255) {
      return const_strlen(prefix) + intLength(static_cast<int>(maxGpio)) +
             const_strlen(middle) + intLength(static_cast<int>(maxDevices)) + 1; // term "+1" is tailing exclamation mark
    }

    static void build(char *buffer, uint8_t gpio, uint8_t deviceCount) {
      size_t pos = 0;
      writeString(buffer, pos, prefix);
      writeInt(buffer, pos, static_cast<int>(gpio));
      writeString(buffer, pos, middle);
      writeInt(buffer, pos, static_cast<int>(deviceCount));
      writeChar(buffer, pos, '!');
      buffer[pos] = '\0';
    }
  };

  // Error 2: Incompatible device address
  struct IncompatibleAddressError {
    static constexpr const char *prefix = " Incompatible device address "; // Stored in flash

    static constexpr size_t worstCaseLength() {
      return const_strlen(prefix) + deviceAddressStringLength() + 1; // term "+1" is tailing exclamation mark
    }

    static void build(char *buffer, const DeviceAddress addr) {
      size_t pos = 0;
      writeString(buffer, pos, prefix);
      writeDeviceAddress(buffer, pos, addr);
      writeChar(buffer, pos, '!');
      buffer[pos] = '\0';
    }
  };

  // Error 3: Unknown device type
  struct UnknownDeviceError {
    static constexpr const char *prefix = " Unknown device type at address "; // Stored in flash

    static constexpr size_t worstCaseLength() {
      return const_strlen(prefix) + deviceAddressStringLength() + 1; // term "+1" is tailing exclamation mark
    }

    static void build(char *buffer, const DeviceAddress addr) {
      size_t pos = 0;
      writeString(buffer, pos, prefix);
      writeDeviceAddress(buffer, pos, addr);
      writeChar(buffer, pos, '!');
      buffer[pos] = '\0';
    }
  };

  // Error 4: Sensor disconnected after init
  struct SensorDisconnectedError {
    static constexpr const char *prefix = " DS18B20 Temp Sensor ";                    // Stored in flash
    static constexpr const char *suffix = " still disconnected after initialization"; // Stored in flash

    static constexpr size_t worstCaseLength() {
      return const_strlen(prefix) + deviceAddressStringLength() + const_strlen(suffix) + 1; // term "+1" is tailing exclamation mark
    }

    static void build(char *buffer, const DeviceAddress addr) {
      size_t pos = 0;
      writeString(buffer, pos, prefix);
      writeDeviceAddress(buffer, pos, addr);
      writeString(buffer, pos, suffix);
      writeChar(buffer, pos, '!');
      buffer[pos] = '\0';
    }
  };

  // Error 5: Parasite power mode
  struct ParasitePowerError {
    static constexpr const char *prefix = " DS18B20 Temp Sensor ";             // Stored in flash
    static constexpr const char *suffix = " is reporting parasite power mode"; // Stored in flash

    static constexpr size_t worstCaseLength() {
      return const_strlen(prefix) + deviceAddressStringLength() + const_strlen(suffix) + 1; // term "+1" is tailing exclamation mark
    }

    static void build(char *buffer, const DeviceAddress addr) {
      size_t pos = 0;
      writeString(buffer, pos, prefix);
      writeDeviceAddress(buffer, pos, addr);
      writeString(buffer, pos, suffix);
      writeChar(buffer, pos, '!');
      buffer[pos] = '\0';
    }
  };

  // Error 6: Precision setting failed
  struct PrecisionSettingError {
    static constexpr const char *part1 = " Setting precision of DS18B20 Temp Sensor "; // Stored in flash
    static constexpr const char *part2 = " to ";                                       // Stored in flash
    static constexpr const char *part3 = " bits failed; sensor reports ";              // Stored in flash
    static constexpr const char *part4 = " bits!";                                     // Stored in flash

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
    static constexpr const char *prefix = " Temperature read failed with value "; // Stored in flash

    static constexpr size_t worstCaseLength() {
      // Use the maximum length that writeFloat can produce, which handles all possible
      // float values including special cases (nan, inf, overflow) and malfunction scenarios.
      // For 32-bit int: maxFloatStringLength() = 14 (e.g., "-2147483647.99")
      // For 64-bit int: maxFloatStringLength() = 23
      return const_strlen(prefix) + maxFloatStringLength() + 1; // term "+1" is tailing exclamation mark
    }

    static void build(char *buffer, float value) {
      size_t pos = 0;
      writeString(buffer, pos, prefix);
      writeFloat(buffer, pos, value);
      writeChar(buffer, pos, '!');
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

} // namespace ErrorMessages
