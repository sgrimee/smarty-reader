/* debug_helpers.h - Simple macros for conditional debugging. */

#ifndef debug_helpers_h
#define debug_helpers_h

// clang-format off
#ifndef SMARTY_DEBUG
  #define DEBUG_BEGIN(...)
  #define DEBUG_PRINT(...)
  #define DEBUG_PRINTLN(...)
  #define DEBUG_PRINTF(...)
#else
  #define DEBUG_BEGIN(...) Serial1.begin(__VA_ARGS__)
  #define DEBUG_PRINT(...) Serial1.print(__VA_ARGS__)
  #define DEBUG_PRINTLN(...) Serial1.println(__VA_ARGS__)  
  #define DEBUG_PRINTF(...) Serial1.printf(__VA_ARGS__)
#endif
// clang-format on

#endif // debug_helpers_h