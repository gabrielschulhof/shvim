#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "debug.h"

void debug(const char* fmt, ...) {
  FILE* pfile = fopen("/Users/gschulhof/dev/shvim/log", "a");
  if (!pfile) {
    fprintf(stderr, "Failed to open log file\n");
    abort();
  }
  DEBUG(va_list va);
  DEBUG(va_start(va, fmt));
  DEBUG(vfprintf(pfile, fmt, va));
  DEBUG(fflush(pfile));
  DEBUG(va_end(va));
  fclose(pfile);
}
