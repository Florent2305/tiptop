#include <stdio.h>

#include "version.h"


void print_version()
{
  printf("tiptop was compiled on \"%s\" by host \"%s\".\n",
         compile_date, compile_host);
}
