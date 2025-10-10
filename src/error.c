/* error.c -- error handler
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

#include "error.h"
#include "util.h"

void fatal_error(const char *fmt, ...) {
  va_list list;
  va_start(list, fmt);
  fprintf(stderr, "FATAL ERROR: ");
  vfprintf(stderr, fmt, list);
  va_end(list);
  fprintf(stderr, "\n");
  exit(1);
}
