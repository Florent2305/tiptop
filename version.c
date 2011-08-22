#include <stdio.h>

#include "version.h"


void print_version()
{
  printf("tiptop was compiled on \"%s\" on host \"%s\".\n",
         compile_date, compile_host);
}
