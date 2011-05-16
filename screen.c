#include <assert.h>
#include <inttypes.h>

#ifdef KERNEL31
#include <linux/perf_counter.h>
#else
#include <linux/perf_event.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "screen.h"
#include "screens-intel.h"
#include "utils.h"

static const int alloc_chunk = 10;

static int num_screens = 0;
static int num_alloc_screens = 0;
static screen_t** screens = NULL;


screen_t* new_screen(const char* name)
{
  screen_t* the_screen = malloc(sizeof(screen_t));
  the_screen->id = num_screens;
  the_screen->name = strdup(name);
  the_screen->num_counters = 0;
  the_screen->num_alloc_counters = alloc_chunk;
  the_screen->counters = malloc(alloc_chunk * sizeof(counter_t));
  the_screen->num_columns = 0;
  the_screen->num_alloc_columns = alloc_chunk;
  the_screen->columns = malloc(alloc_chunk * sizeof(column_t));

  if (num_screens >= num_alloc_screens) {
    num_alloc_screens += alloc_chunk;
    screens = realloc(screens, num_alloc_screens * sizeof(screen_t*));
  }
  screens[num_screens] = the_screen;

  num_screens++;
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
  /* should check if already in list ?? */
  s->counters[n].type = type;
  s->counters[n].config = config;
  s->num_counters++;
  return n;
}


static int add_column_tmpl(screen_t* s, enum comput_type typ,
                           char* header, char* format,
                           int counter1, int counter2)
{
  col_comput_t data;
  int col_width;
  int n = s->num_columns;

  if (n == s->num_alloc_columns) {
    s->columns = realloc(s->columns, sizeof(column_t) * (n + alloc_chunk));
    s->num_alloc_columns += alloc_chunk;
  }
  data.type = typ;
  data.param1 = counter1;
  data.param2 = counter2;
  s->columns[n].header = header;
  s->columns[n].format = format;

  /* setup an empty field with proper width */
  col_width = strlen(header);
  s->columns[n].empty_field = malloc(col_width + 1);
  memset(s->columns[n].empty_field, ' ', col_width);
  s->columns[n].empty_field[col_width] = '\0';

  /* setup an error field with proper width */
  s->columns[n].error_field = malloc(col_width + 1);
  memset(s->columns[n].error_field, ' ', col_width - 1);
  s->columns[n].error_field[col_width - 1] = '?';
  s->columns[n].error_field[col_width] = '\0';

  s->columns[n].data = data;
  s->num_columns++;
  return n;
}


int add_column_cpu(screen_t* s, char* header, char* format)
{
  return add_column_tmpl(s, CPU_TOT, header, format, -1, -1);
}


int add_column_cpu_s(screen_t* s, char* header, char* format)
{
  return add_column_tmpl(s, CPU_SYS, header, format, -1, -1);
}


int add_column_cpu_u(screen_t* s, char* header, char* format)
{
  return add_column_tmpl(s, CPU_USER, header, format, -1, -1);
}


int add_column_raw(screen_t* s, char* header, char* format, int counter)
{
  return add_column_tmpl(s, COMPUT_RAW, header, format, counter, -1);
}


int add_column_raw_m(screen_t* s, char* header, char* format, int counter)
{
  return add_column_tmpl(s, COMPUT_RAW_M, header, format, counter, -1);
}


int add_column_abs(screen_t* s, char* header, char* format, int counter)
{
  return add_column_tmpl(s, COMPUT_ABS, header, format, counter, -1);
}


int add_column_ratio(screen_t* s,
		     char* header, char* format,
		     int counter1, int counter2)
{
  return add_column_tmpl(s, COMPUT_RATIO, header, format, counter1, counter2);
}


int add_column_percent(screen_t* s,
		       char* header, char* format,
		       int counter1, int counter2)
{
  return add_column_tmpl(s, COMPUT_PERCENT, header, format, counter1, counter2);
}


/* This is the default screen, it uses only counters defined in the
   Linux header file. */
screen_t* default_screen()
{
  int cycle, insn, miss, br, bus;
  screen_t* s = new_screen("default");

  /* setup counters */
  cycle = add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
  insn =  add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
  miss =  add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES);
  br =    add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES);
  bus =   add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_BUS_CYCLES);

  /* add columns */
  add_column_cpu(s, " %CPU", "%5.1f");
  add_column_cpu_s(s, " %SYS", "%5.1f");
  add_column_raw_m(s, "  Mcycle", "%8.2f", cycle);
  add_column_raw_m(s, "  Minstr", "%8.2f", insn);
  add_column_ratio(s, " IPC", "%4.2f", insn, cycle);
  add_column_percent(s, "%MISS", "%5.1f", miss, insn);
  add_column_percent(s, "%BMIS", "%5.1f", br, insn);
  add_column_ratio(s, " %BUS", "%5.1f", bus, insn);
  return s;
}


/* Temporary screen */
screen_t* micro()
{
  int insn, cycle;
  screen_t* s = new_screen("MICRO");

  /* setup counters */
  cycle = add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
  insn =  add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);

  /* add columns */
  add_column_cpu(s, " %CPU", "%5.1f");
  add_column_raw_m(s, "  Mcycle", "%8.2f", cycle);
  add_column_raw_m(s, "  Minstr", "%8.2f", insn);
  add_column_abs(s, "     tot instr", "%14lld", insn);
  add_column_ratio(s, " IPC", "%4.2f", insn, cycle);

  return s;
}


void init_screen()
{
  default_screen();
#if defined(TARGET_X86)
  nehalem_fp();
  nehalem_mem();
  nehalem_br();
  nehalem_app();
#endif
  micro();
}


screen_t* get_screen(int num)
{
  if (num >= num_screens)
    return NULL;
  return screens[num];
}


char* gen_header(screen_t* s, int show_user, int timestamp)
{
  char* hdr;
  int   num_cols, i;
  int   cur_alloc;

  if (timestamp)
    hdr = str_init("timest ", &cur_alloc);
  else
    hdr = str_init("", &cur_alloc);

  if (show_user)
    hdr = str_append(hdr, &cur_alloc, "  PID  user      ");
  else
    hdr = str_append(hdr, &cur_alloc, "  PID ");

  num_cols = s->num_columns;
  for(i=0; i < num_cols; i++) {
    hdr = str_append(hdr, &cur_alloc, " ");
    hdr = str_append(hdr, &cur_alloc, s->columns[i].header);
  }
  hdr = str_append(hdr, &cur_alloc, " COMMAND");
  return hdr;
}


int get_num_screens()
{
  return num_screens;
}


void list_screens()
{
  int i;
  printf("Available screens:\n");
  for(i=0; i < num_screens; i++) {
    printf("%2d: %s\n", i, screens[i]->name);
  }
}


/* Delete one screen. */
void delete_screen(screen_t* s)
{
  int i;

  assert(s);
  free(s->name);
  free(s->counters);
  for(i=0; i < s->num_columns; i++) {
    free(s->columns[i].error_field);
    free(s->columns[i].empty_field);
  }
  free(s->columns);
  free(s);
}


/* Delete all screens. */
void delete_screens()
{
  int i;
  for(i=0; i < num_screens; i++) {
    delete_screen(screens[i]);
  }
  free(screens);
}
