#ifndef _SCREEN_H
#define _SCREEN_H

#include <inttypes.h>


typedef struct {
  uint32_t  type;
  uint64_t  config;
} counter_t;


typedef struct {
  char* header;
  char* format;  /* as in printf */
  
} column_t;


typedef struct {
  int     id;
  char*   name;
  int        num_counters;
  int        num_alloc_counters;
  counter_t* counters;
  int        num_columns;
  int        num_alloc_columns;
  column_t*  columns;
} screen_t;

screen_t* new_screen(const char* name);
int add_counter(screen_t* s, int32_t type, int64_t config);
int add_column(screen_t* s);
screen_t* default_screen();

void delete_screen(screen_t* s);



#endif  /* _SCREEN_H */
