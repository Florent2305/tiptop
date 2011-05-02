#ifndef _SCREEN_H
#define _SCREEN_H

#include <inttypes.h>


typedef struct {
  uint32_t  type;
  uint64_t  config;
} counter_t;


enum comput_type {
  CPU_TOT,       /* %cpu */
  CPU_SYS,       /* %cpu spent in system */
  CPU_USER,      /* %cpu spent in user land */
  COMPUT_RAW,
  COMPUT_RAW_M,  /* RAW, print in millions */
  COMPUT_ABS,    /* absolute number, from the beginning */
  COMPUT_RATIO,
  COMPUT_PERCENT
};

typedef struct {
  enum comput_type type : 8;
  int  param1 : 8;
  int  param2 : 8;
} col_comput_t;

typedef struct {
  char* header;
  char* format;  /* as in printf */
  char* empty_field;
  char* error_field;
  col_comput_t data;
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
int add_column_cpu(screen_t* s, char*, char*);
int add_column_raw(screen_t* s, char*, char*, int);
int add_column_raw_m(screen_t* s, char*, char*, int);
int add_column_abs(screen_t* s, char*, char*, int);
int add_column_ratio(screen_t*, char*, char*, int, int);
int add_column_percent(screen_t*, char*, char*, int, int);

screen_t* default_screen();
void init_screen();
screen_t* get_screen(int);

int get_num_screens();

void list_screens();

char* gen_header(screen_t* s, int show_user);

void delete_screen(screen_t* s);


#endif  /* _SCREEN_H */
