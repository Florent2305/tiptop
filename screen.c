#include <assert.h>
#include <inttypes.h>

#ifdef KERNEL31
#include <linux/perf_counter.h>
#else
#include <linux/perf_event.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "screen.h"

static const int alloc_chunk = 10;

static int num_screens = 0;

screen_t* new_screen(const char* name)
{
  screen_t* the_screen = malloc(sizeof(screen_t));
  the_screen->id = num_screens;
  num_screens++;
  the_screen->name = strdup(name);
  the_screen->num_counters = 0;
  the_screen->num_alloc_counters = alloc_chunk;
  the_screen->counters = malloc(alloc_chunk * sizeof(counter_t));
  the_screen->num_columns = 0;
  the_screen->num_alloc_columns = alloc_chunk;
  the_screen->columns = malloc(alloc_chunk * sizeof(counter_t));
  return the_screen;
}


/* return position of counter */
int add_counter(screen_t* s, int32_t type, int64_t config)
{
  int n = s->num_counters;
  /* check max available hw counter */
  if (n == s->num_alloc_counters) {
    s->counters = realloc(s->counters, sizeof(counter_t) * (n + alloc_chunk));
    n += alloc_chunk;
    s->num_counters += alloc_chunk;
  }
  /* check if already in list ?? */
  s->counters[n].type = type;
  s->counters[n].config = config;
  s->num_counters++;
  return n;
}


int add_column(screen_t* s)
{
  return 0;
}


screen_t* default_screen()
{
  screen_t* s = new_screen("default");
  add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
  add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
  add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES);
  add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES);
  /* add columns */

  return s;
}


void delete_screen(screen_t* s)
{
  assert(s);
  free(s->name);
  free(s->counters);
  free(s->columns);
  free(s);
}
