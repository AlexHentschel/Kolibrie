#pragma once

/* debug_do: Execute code only when DEBUG is defined, without heap allocation.
 * 
 * This utility wraps debug-only code in a lambda to avoid preprocessor directives scattered
 * throughout the codebase. When used correctly, it guarantees zero heap allocation.
 * 
 * REQUIREMENTS FOR CALLER (to prevent heap allocation, i.e. risk of memory fragmentation):
 * ═════════════════════════════════════════════════════════════════════════════════════════
 * 1. Pass lambda directly to `debug_do` - do NOT store it in a variable first
 * 2. Do NOT wrap the lambda in `std::function` or similar type-erasing containers
 * 3. Lambda must be invoked immediately (which `debug_do` does) - do NOT return or store it
 * 4. All capture patterns are safe ([], [&], [=], [&var], [var], etc.):
 *    • By reference [&] or [&var] - always stack-allocated, most efficient
 *    • By value [=] or [var] - copies are stack-allocated
 *    • For primitives/pointers: by-value and by-reference have similar performance
 *    • For large objects: prefer by-reference [&] to avoid stack overflow
 * 
 * LAMBDA CAPTURE PATTERNS (all are heap-safe with `debug_do`):
 * ─────────────────────────────────────────────────────────────────────────────────────────
 *   // 1. No capture [] - stateless lambda, no access to surrounding variables
 *   debug_do([]() {
 *     Serial.println("Debug message");
 *   });
 * 
 *   // 2. Capture all by reference [&] - can read/write all surrounding variables
 *   //    Most common pattern. Efficient for primitives and objects.
 *   debug_do([&]() {
 *     Serial.print(currentMicros);
 *   });
 * 
 *   // 3. Capture all by value [=] - makes copies of all used variables
 *   //    Safe for primitives, but copies large objects (use with caution)
 *   debug_do([=]() {
 *     Serial.print(currentMicros);  // uses a copy
 *   });
 * 
 *   // 4. Capture specific variables by reference [&var]. Explicit and clear about dependencies.
 *   debug_do([&currentMicros]() {
 *     Serial.print(currentMicros);
 *   });
 * 
 *   // 5. Capture specific variables by value [var]. Makes a copy of the specified variable.
 *   debug_do([currentMicros]() {
 *     Serial.print(currentMicros);  // uses a copy
 *   });
 * 
 *   // 6. Mix by-reference and by-value captures. Flexible but be explicit about what you're capturing.
 *   //    Flexible but be explicit about what you're capturing
 *   debug_do([&currentMicros, startMicros]() {
 *     Serial.print(currentMicros - startMicros);  // currentMicros by ref, startMicros by value
 *   });
 * 
 *   // 7. Capture all by reference, except specific by value [&, var]
 *   debug_do([&, startMicros]() {
 *     Serial.print(currentMicros - startMicros);  // currentMicros by ref, startMicros by value
 *   });
 * 
 *   // 8. Capture all by value, except specific by reference [=, &var]
 *   debug_do([=, &currentMicros]() {
 *     Serial.print(currentMicros);  // currentMicros by ref, others by value
 *   });
 * 
 * UNSAFE PATTERNS (cause heap allocation or other issues):
 * ─────────────────────────────────────────────────────────────────────────────────────────
 *   // BAD: storing lambda in `std::function` causes heap allocation:
 *   std::function<void()> f = []() { Serial.println("Bad"); };
 *   debug_do(f);  // ✗ HEAP ALLOCATION
 * 
 *   // BAD: auto-deduced type then passed elsewhere
 *   auto lambda = []() { Serial.println("Bad"); };
 *   someOtherFunction(lambda);  // ✗ depends on someOtherFunction's implementation
 * 
 *   // GOOD: direct pass-through to debug_do
 *   auto lambda = []() { Serial.println("Good"); };
 *   debug_do(lambda);  // ✓ safe; lambda still stack-allocated
 * 
 * WHY THIS IS HEAP-SAFE:
 * ─────────────────────────────────────────────────────────────────────────────────────────
 * • Template with forwarding reference (Func &&f) preserves the lambda's actual type
 * • No type erasure means no heap allocation for type information
 * • Lambda objects themselves are always stack-allocated in C++ unless explicitly stored
 *   in heap-allocating containers (`std::function`, `new`, smart pointers, etc.)
 * • Immediate invocation ensures lambda doesn't escape scope
 * 
 * PERFORMANCE NOTE:
 * ─────────────────────────────────────────────────────────────────────────────────────────
 * • With optimization enabled, the compiler inlines everything to zero overhead
 * • [&] vs [&var1, &var2]: No performance difference - compiler captures only what's used
 * • In DEBUG builds: negligible overhead from lambda wrapper
 * • In non-DEBUG builds: entire call is optimized away (zero code generated)
 */
template <typename Func>
inline void debug_do(Func &&f) {
#if defined(DEBUG)
  f();
#endif
}
