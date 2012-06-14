/*
 * This file is part of tiptop.
 *
 * Author: Antoine NAUDIN
 * Copyright (c) 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */


/*
 * This file is dedicated to print in a file the actual configuration.
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "screen.h"
#include "utils-expression.h"
#include "write-config.h"


static FILE* out = NULL;

static const char* const tok_opt_sta = "\t\t<option name=\"";
static const char* const tok_opt_mid = "\" value=\"";
static const char* const tok_opt_end ="\" />";

static int build_option_string(char * name, char* value)
{
  if (value == NULL)
    return 0;
  return fprintf(out , "%s%s%s%s%s\n",tok_opt_sta, name, tok_opt_mid, value, tok_opt_end);
}

static int build_option_int(char * name, int value)
{
  return fprintf(out, "%s%s%s%d%s\n",tok_opt_sta, name, tok_opt_mid, value, tok_opt_end);
}

static int build_option_float(char * name, float value)
{
  return fprintf(out, "%s%s%s%f%s\n",tok_opt_sta, name, tok_opt_mid, value, tok_opt_end);
}

static const char* const opt_sta = "\t<options>";
static const char* const opt_clo = "\t</options>";

static int build_options(struct option* opt)
{
  if (fprintf(out, "%s\n", opt_sta) < 0)
    return -1;
  if (build_option_float("cpu_threshold", opt->cpu_threshold ) < 0)
    return -1;
  if (build_option_float("delay", opt->delay ) < 0)
    return -1;
  if (build_option_string("watch_name", opt->watch_name) < 0)
    return -1;
  if (build_option_int("max_iter", opt->max_iter) < 0)
    return -1;
  if (build_option_string("only_name", opt->only_name) < 0)
    return -1;
  if (build_option_int("only_pid",(int) opt->only_pid) < 0)
    return -1;
  if  (build_option_int("debug", opt->debug) < 0)
    return -1;
  if (build_option_int("batch", opt->batch) < 0)
    return -1;
  if (fprintf(out, "%s\n", opt_clo) < 0)
    return -1;

  return 0;
}


static const char* const cou_sta  = "\t\t<counter alias=\"";
static const char* const cou_mid1 = "\" config=\"";
static const char* const cou_mid2 = "\" type=\"";
/* static const char* const cou_arch = "\" arch=\""; */
/* static const char* const cou_model = "\" model=\""; */
static const char* const cou_clo  = "\" />";
static char* const default_type  = "PERF_TYPE_HW";


static int build_counter(counter_t* c)
{
  int alloc_c=0, alloc_t=0;
  char* config = NULL;
  char* type = NULL;

  config = get_counter_config_name(c->config);
  type = get_counter_type_name(c->type);

  if (config == NULL) {
    /* Hexadecimal Convertion */
    alloc_c++;
    config = malloc(sizeof(char)*3);
    sprintf(config, "0x%"PRIx64, c->config);
  }
  if (type == NULL && c->type != -1) {
    /* Hexadecimal Convertion */
    alloc_t++;
    type = malloc(sizeof(char)*3);
    sprintf(type,"0x%x", c->type);
  }
  else if (type == NULL && c->type == -1)
    type = default_type;

  if (fprintf(out, "%s%s%s%s%s%s%s\n", cou_sta, c->alias, cou_mid1, config,
              cou_mid2, type, cou_clo) < 0)
    goto end;

  if (alloc_c)
    free(config);
  if (alloc_t)
    free(type);
  return 0;

 end:
  if(alloc_c)
    free(config);
  if(alloc_t)
    free(type);
  return -1;
}


static const char* const col_sta  = "\t\t<view header=\"";
static const char* const col_format = "\" format=\"";
static const char* const col_desc = "\" desc=\"";
static const char* const col_expr = "\" expr=\"";
static const char* const col_clo  = "\" />";

static int build_view(column_t* c)
{
  if (fprintf(out, "%s%s%s%s%s%s%s", col_sta, c->header, col_format,c->format, col_desc, c->description, col_expr) < 0)
    return -1;
  if (build_expression(c->expression, out) < 0)
    return -1;
  if (fprintf(out, "%s\n", col_clo) < 0)
    return -1;
  return 0;
 }

static const char* const screen_sta = "\t<screen name=\"";
static const char* const screen_mid = "\" desc=\"";
static const char* const screen_clo = "\">";
static const char* const screen_end = "\t</screen>";

static int build_screen(screen_t* s)
{
  int i;

  if (s->desc == NULL) {
    if (fprintf(out, "%s%s%s\n",screen_sta, s->name, screen_clo) < 0)
      return -1;
  }
  else {
    if (fprintf(out, "%s%s%s%s%s\n",screen_sta, s->name, screen_mid, s->desc, screen_clo) < 0)
      return -1;
  }
  /* Print Counters*/
  for(i=0;i<s->num_counters;i++)
    if (build_counter(&s->counters[i]) < 0)
      return -1;
  /* Print Views or Columns */
  for(i=0;i<s->num_columns;i++)
    if (build_view(&s->columns[i]) < 0)
      return -1;

  if (fprintf(out, "%s\n", screen_end) < 0)
    return -1;
  return 0;
}


static int build_screens(screen_t** s, int nbs)
{
  int i;

  if (nbs == 0)
    return 0;

  for(i=0; i < nbs; i++)
    if (build_screen(s[i]) < 0)
      return -1;

  return 0;
}

static const char* const tip_sta ="<tiptop>";
static const char* const tip_clo = "</tiptop>";
static const char* const introduction =
"<!-- tiptop configuration file -->\n"
"\n"
"<!-- Rename this file to .tiptoprc,                                       -->\n"
"<!-- and place it either in your current directory, the location          -->\n"
"<!-- specified in $TIPTOP, or in your $HOME.                              -->\n"
;

/* Main function to export a configuration file */
int build_configuration(screen_t** sc, int num_screens, struct option* o,
                        FILE* file)
{
  fflush(file);
  out = file;

  if (fprintf(out, "%s\n%s\n", introduction, tip_sta) < 0)
    return -1;
  if (build_options(o) < 0)
    return -1;
  if (build_screens(sc, num_screens) < 0)
    return -1;
  if (fprintf(out, "%s\n", tip_clo) < 0)
    return -1;
  return 0;
}
