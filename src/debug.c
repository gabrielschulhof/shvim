#include <stdio.h>
#include <stdarg.h>
#include "debug.h"

void debug(const char* fmt, ...) {
  DEBUG(FILE* output = fopen("/home/gschulhof/shvim/shvim.log", "a"));
  DEBUG(va_list va);
  DEBUG(va_start(va, fmt));
  DEBUG(vfprintf(output, fmt, va));
  DEBUG(fflush(output));
  DEBUG(fclose(output));
  DEBUG(va_end(va));
}
