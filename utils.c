#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "utils.h"

char* str_init(const char* str, int* curr_alloc)
{
  char* res = strdup(str);
  *curr_alloc = strlen(res) + 1;  /* including final '\0' */
  return res;
}


/* Append string 'second' to string 'first'. 'cur_alloc' points to the
   currently allocated size for the first string. It is updated. */
char* str_append(char* first, int* curr_alloc, const char* const second)
{
  int len1, len2;
  len1 = strlen(first);
  len2 = strlen(second);
  if (len1 + len2 + 1 > *curr_alloc) {
    *curr_alloc = len1 + len2 + 1;
    first = realloc(first, *curr_alloc);
  }
  strcat(first, second);
  return first;
}
