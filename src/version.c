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

#include "version.h"


void print_version()
{
  printf("This is tiptop version %s.\n", tiptop_version);
  printf("tiptop was compiled on \"%s\"\non host \"%s\".\n",
         compile_date, compile_host);
}

void print_legal()
{
  printf("tiptop %s.\n", tiptop_version);
  printf("Copyright (C) 2011, 2012 Inria.\n");
  printf("This is free software. No warranty.\n");
}
