#include <stdio.h>
#include <stdarg.h>
#include "debug.h"

void debug(const char* fmt, ...) {
  DEBUG(va_list va);
  DEBUG(va_start(va, fmt));
  DEBUG(vfprintf(stderr, fmt, va));
  DEBUG(fflush(stderr));
  DEBUG(va_end(va));
}
