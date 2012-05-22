#include <assert.h>
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


int parse_doc(char* file, struct option* op);
void parse_screen(xmlDocPtr doc, xmlNodePtr cur);
void parse_counters(xmlDocPtr doc, xmlNodePtr cur);
/* void parse_options(xmlDocPtr doc, xmlNodePtr cur); */
