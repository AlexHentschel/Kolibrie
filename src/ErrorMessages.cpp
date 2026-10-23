#include "ErrorMessages.h"

namespace ErrorMessages {
  // Static buffer for error message construction
  // This buffer is reused to avoid heap allocation and fragmentation
  char errorBuffer[BUFFER_SIZE];

  // getBuffer returns a pointer to the error buffer
  // SINGLETON INSTANCE: The buffer is reused to avoid memory fragmentation and improve performance
  //
  // WARNING: This function is NOT thread-safe. Only one error message can be built at a time.
  // If using in a multi-threaded environment, proper synchronization must be added.
  const char *getBuffer() {
    return errorBuffer;
  }
}
