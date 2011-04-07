#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "screen.h"


static int num_screens = 0;

screen_t* new_screen(const char* name)
{
  screen_t* the_screen = malloc(sizeof(screen_t));
  the_screen->id = num_screens;
  num_screens++;
  the_screen->name = strdup(name);
  the_screen->num_counters = 0;
  the_screen->counters = NULL;
  return the_screen;
}


/* return position of counter */
int add_counter(screen_t* s, int32_t type, int64_t config)
{
  int n = s->num_counters;
  /* check max counter */
  s->counters[n].type = type;
  s->counters[n].config = config;
  s->num_counters++;
  return n;
}


void delete_screen(screen_t* s)
{
  assert(s);
  free(s->name);
  free(s);
}
