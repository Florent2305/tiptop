#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "options.h"

extern struct option options;

static FILE* debug_file = NULL;


void debug_printf(char* fmt, ...)
{
  va_list args;

  if (!options.debug)
    return;

  /* open file the first time we get here. */
  if (!debug_file) {
    debug_file = fopen("debug", "w");
    if (!debug_file) {
      perror("fopen");
      fprintf(stderr, "Cannot open debug file.\n");
      exit(EXIT_FAILURE);
    }
  }

  va_start(args, fmt);
  vfprintf(debug_file, fmt, args);
}
