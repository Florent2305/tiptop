/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2011 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conf.h"
#include "options.h"

#define LINE_SIZE 100

static const char* const config_file = ".tiptoprc";

/* Read configuration file. First check the TIPTOP environment
   variable, then the local directory, finally $HOME. */
int read_config(struct option* options)
{
  FILE* f = NULL;
  char* val = getenv("TIPTOP");
  if (val) {
    f = fopen(val, "r");
  }
  if (!f) {
    f = fopen(config_file, "r");
  }
  if (!f) {
    char* home = getenv("HOME");
    if (home) {
      char* path;
      path = malloc(strlen(home) + strlen(config_file) + 2);
      sprintf(path, "%s/%s", home, config_file);
      f = fopen(path, "r");
      free(path);
    }
  }

  if (f) {
    /* read config file */

    /* ====== VERY PRELIMINARY ================== */

    char line[LINE_SIZE];
    while (fgets(line, LINE_SIZE, f)) {
      if (line[0] == '#')  /* comment */
        continue;

      if (strncmp(line, "delay", 5) == 0)
        sscanf(line, "delay: %f", &options->delay);

      if (strncmp(line, "cpu-threshold", 13) == 0)
        sscanf(line, "cpu_threshold: %f", &options->cpu_threshold);

      if (strncmp(line, "show-threads", 12) == 0)
        options->show_threads = 1;

      if (strncmp(line, "show-idle", 9) == 0)
        options->idle = 1;

      if (strncmp(line, "sticky", 6) == 0)
        options->sticky = 1;
    }
    fclose(f);
  }
  return 0;
}
