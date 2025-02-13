#include "utils.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

void fatalf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  assert(0);
}