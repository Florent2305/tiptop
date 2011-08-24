#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conf.h"


static const char* const config_file = ".tiptop";

/* Read configuration file. First check the TIPTOP environment
   variable, then the local directory, finally $HOME. */
int read_config()
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



    fclose(f);
  }
  return 0;
}
