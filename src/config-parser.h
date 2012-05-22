/*
 * This file is part of tiptop.
 *
 * Author: Antoine NAUDIN
 *
 */

#ifndef _CONFIG_PARSER_H
#define _CONFIG_PARSER_H

#include "config.h"

int parse_doc(char* file, struct option* op);


#if HAVE_LIBXML2

#include <libxml/parser.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct conf_screen conf_screen;
typedef struct conf_counter conf_counter;
typedef struct conf_view conf_view;

struct conf_counter {
  char* alias;
  char* name;
  int value;
};


struct conf_view {
  char* header;
  char* desc;
  char* expr;
  char* format;
  conf_counter listC[512];
  int nbC;
};


struct conf_screen {
  char* name;
  conf_view listV[512];
  int nbV;
};

/* void option_update(xmlChar* name, xmlChar* val); */
void counter_update(xmlChar* alias, xmlChar* name, xmlChar* value);
void view_update(xmlChar* header, xmlChar* format, xmlChar* desc, xmlChar* expr);
void screen_update(xmlChar *name);


void parse_screen(xmlDocPtr doc, xmlNodePtr cur);
void parse_counters(xmlDocPtr doc, xmlNodePtr cur);
/* void parse_options(xmlDocPtr doc, xmlNodePtr cur); */

#endif  /* HAVE_LIBXML2 */

#endif  /* _CONFIG_PARSER_H */
