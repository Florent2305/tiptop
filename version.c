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

#include "version.h"


void print_version()
{
  printf("This is tiptop version " TIPTOP_VERSION ".\n");
  printf("tiptop was compiled on \"%s\" on host \"%s\".\n",
         compile_date, compile_host);
}

void print_legal()
{
  printf("tiptop\n");
  printf("Copyright (C) 2011 Inria\n");
  printf("This is free software. No warranty.\n");
}
