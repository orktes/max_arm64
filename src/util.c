/* util.c -- misc utility functions
 *
 * Copyright (C) 2021 fgsfds, Andy Nguyen (original code for Switch)
 * Copyright (C) 2025 Jaakko Lukkari
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "util.h"

#ifdef DEBUG_LOG

// For ARM64 Linux, we don't need special network initialization
// Debug output will go to stdout/log file directly

void userAppInit(void) {
  // No special initialization needed for ARM64 Linux
}

void userAppExit(void) {
  // No special cleanup needed for ARM64 Linux
}

#endif

int debugPrintf(char *text, ...) {
#ifdef DEBUG_LOG
  va_list list;

  FILE *f = fopen(LOG_NAME, "a");
  if (f) {
    va_start(list, text);
    vfprintf(f, text, list);
    va_end(list);
    fclose(f);
  }

  va_start(list, text);
  vprintf(text, list);
  va_end(list);
#endif
  return 0;
}

int ret0(void) {
  // Uncomment the line below to debug which stub functions are called
  // debugPrintf("ret0() called\n");
  return 0;
}

int ret1(void) { return 1; }

int retm1(void) { return -1; }
