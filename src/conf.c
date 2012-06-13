/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2011, 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "conf.h"
#include "options.h"
#include "xml-parser.h"

static const char* const config_file = ".tiptoprc";

/* Read configuration file.
 *
 * First check the path specified on the command line (if any), then
 * the TIPTOP environment variable, then the local directory, finally
 * $HOME. */

int read_config(struct option* options)
{
  char* path = NULL;
  char* file = NULL;
  int   res;

#if HAVE_LIBXML2
  /* Check path with '-W' in tiptop options */
  if (options->path_conf_file != NULL) {
      path = options->path_conf_file;
    }

  if (!path) {
      /* Check Env. Var.  $TIPTOP */
      path = getenv("TIPTOP");
    }

  if (!path) {
      /* Check Current Folder */
      if (access(config_file, R_OK) == 0) {
        path = ".";
      }
    }

  if (!path) {
      /* Check $HOME */
      path = getenv("HOME");
    }

  if (!path)
    return -1;

  file = malloc(strlen(path) + strlen(config_file) + 2);
  sprintf(file, "%s/%s", path, config_file);

  if (access(file, R_OK) == -1) {
    free(file);
    return -1;
  }

  res = parse_doc(file, options);
  free(file);
  return res;
  
#else  /* HAVE_LIBXML2 */
  
  fprintf(stderr, "No xml support, cannot read config file.");
  return -1;
  
#endif  /* !HAVE_LIBXML2 */
}
