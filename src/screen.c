/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2011 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

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

#include "options.h"
#include "screen.h"

static const int alloc_chunk = 10;

static int num_screens = 0;
static int num_alloc_screens = 0;
static screen_t** screens = NULL;


screen_t* new_screen(const char* const name)
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
int add_counter(screen_t* const s, int32_t type, int64_t config)
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


static int add_column_tmpl(screen_t* const s, enum comput_type typ,
                           char* header, char* format,
                           int counter1, int counter2,
                           char* desc)
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
  if (desc)
    s->columns[n].description = desc;
  else
    s->columns[n].description = "(unknown)";

  s->num_columns++;
  return n;
}


int add_column_cpu(screen_t* const s, char* header, char* format)
{
  return add_column_tmpl(s, CPU_TOT, header, format, -1, -1,
                         "Fraction of time spent executing process");
}


int add_column_cpu_s(screen_t* const s, char* header, char* format)
{
  return add_column_tmpl(s, CPU_SYS, header, format, -1, -1,
                         "Fraction of time spent in kernel mode");
}


int add_column_cpu_u(screen_t* const s, char* header, char* format)
{
  return add_column_tmpl(s, CPU_USER, header, format, -1, -1,
                         "Fraction of time spent in user mode");
}


int add_column_proc_id(screen_t* const s, char* header, char* format)
{
  return add_column_tmpl(s, PROC_ID, header, format, -1, -1,
                         "Processor on which task was last seen");
}


int add_column_raw(screen_t* const s, char* header, char* format, int counter,
                   char* desc)
{
  return add_column_tmpl(s, COMPUT_RAW, header, format, counter, -1, desc);
}


int add_column_raw_m(screen_t* const s, char* header, char* format, int counter,
                     char* desc)
{
  return add_column_tmpl(s, COMPUT_RAW_M, header, format, counter, -1, desc);
}


int add_column_abs(screen_t* const s, char* header, char* format, int counter,
                   char* desc)
{
  return add_column_tmpl(s, COMPUT_ABS, header, format, counter, -1,
                         desc);
}


int add_column_abs_m(screen_t* const s, char* header, char* format, int counter,
                   char* desc)
{
  return add_column_tmpl(s, COMPUT_ABS_M, header, format, counter, -1,
                         desc);
}


int add_column_ratio(screen_t* const s,
		     char* header, char* format,
		     int counter1, int counter2,
                     char* desc)
{
  return add_column_tmpl(s, COMPUT_RATIO, header, format, counter1, counter2,
                         desc);
}


int add_column_percent(screen_t* const s,
		       char* header, char* format,
		       int counter1, int counter2,
                       char* desc)
{
  return add_column_tmpl(s, COMPUT_PERCENT, header, format, counter1, counter2,
                         desc);
}


/* This is the default screen, it uses only target-independent
   counters defined in the Linux header file. */
static screen_t* default_screen()
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
  add_column_proc_id(s, " P", "%2d");
  add_column_raw_m(s, "  Mcycle", "%8.2f", cycle, "Cycles (millions)");
  add_column_raw_m(s, "  Minstr", "%8.2f", insn, "Instructions (millions)");
  add_column_ratio(s, " IPC", "%4.2f", insn, cycle,
                   "Executed instructions per cycle");
  add_column_percent(s, " %MISS", "%6.2f", miss, insn,
                     "Cache miss per instruction");
  add_column_percent(s, " %BMIS", "%6.2f", br, insn,
                     "Branch misprediction per instruction");
  add_column_ratio(s, " %BUS", "%5.1f", bus, insn,
                   "Bus cycles per executed instruction");
  return s;
}


static screen_t* branch_pred_screen()
{
  int insn, br, misp;
  screen_t* s;

  s = new_screen("branch prediction");

  /* setup counters */
  insn = add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
  br  =  add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS);
  misp = add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES);

  /* add columns */
  add_column_cpu(s, " %CPU", "%5.1f");
  add_column_percent(s, "  %MISP", "  %5.2f", misp, br,
                     "Mispredictions per 100 branch instructions");
  add_column_percent(s, "  %MIS/I ", "   %5.2f ", misp, insn,
                     "Mispredictions per 100 instructions");
  add_column_percent(s, "%BR/I", " %4.1f", br, insn,
                     "Fraction of branch instructions");
  return s;
}


void init_screen()
{
  default_screen();
  branch_pred_screen();
#if !defined(NOTARGET)
  screens_hook();
#endif
}


screen_t* get_screen(int num)
{
  if (num >= num_screens)
    return NULL;
  return screens[num];
}


char* gen_header(const screen_t* const s, const struct option* options,
                 int width, int active_col)
{
  char* hdr;
  char* ptr;
  int   num_cols, i, written = 0;
  const char sep = ' ';
  const char high_on = '[';
  const char high_off = ']';

  hdr = malloc(width);
  ptr = hdr;

  if (options->show_timestamp && options->batch) {
    written = snprintf(ptr, width, "timest ");
    ptr += written;
    width -= written;
  }

  if (options->show_epoch && options->batch) {
    written = snprintf(ptr, width, "     epoch ");
    ptr += written;
    width -= written;
  }

  if (options->show_user)
    written = snprintf(ptr, width, " %cPID%c user      ",
                       active_col == -1 ? high_on : sep,
                       active_col == -1 ? high_off : sep);
  else
    written = snprintf(ptr, width, " %cPID%c",
                       active_col == -1 ? high_on : sep,
                       active_col == -1 ? high_off : sep);

  ptr += written;
  width -= written;

  num_cols = s->num_columns;
  for(i=0; i < num_cols; i++) {

    /* add space, if it fits */
    if (width >= 2) {
      if (i == active_col)
        ptr[0] = high_on;
      else if ((i-1 == active_col) && (i != 0))
        ptr[0] = high_off;
      else
        ptr[0] = sep;
      ptr[1] = '\0';
      ptr++;
      width--;
    }

    /* add column header */
    written = snprintf(ptr, width, "%s", s->columns[i].header);
    if (written >= width) {
      width = 0;
      break;
    }
    ptr += written;
    width -= written;
  }
  snprintf(ptr, width, "%cCOMMAND%c",
           active_col == s->num_columns-1
                      ? high_off
                      : active_col == s->num_columns ? high_on : sep,
           active_col == s->num_columns ? high_off : sep);
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