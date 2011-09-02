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
  PROC_ID,       /* the processor ID on which this thread was last seen */
  COMPUT_RAW,
  COMPUT_RAW_M,  /* RAW, print in millions */
  COMPUT_ABS,    /* absolute number, from the beginning */
  COMPUT_ABS_M,  /* ABS, in millions */
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
  char* description;
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

screen_t* new_screen(const char* const);
int add_counter(screen_t* const, int32_t type, int64_t config);
int add_column_cpu(screen_t* const, char*, char*);
int add_column_cpu_s(screen_t* const, char*, char*);
int add_column_cpu_u(screen_t* const, char*, char*);
int add_column_proc_id(screen_t* const, char*, char*);
int add_column_raw(screen_t* const, char*, char*, int, char*);
int add_column_raw_m(screen_t* const, char*, char*, int, char*);
int add_column_abs(screen_t* const, char*, char*, int, char*);
int add_column_abs_m(screen_t* const, char*, char*, int, char*);
int add_column_ratio(screen_t* const, char*, char*, int, int, char*);
int add_column_percent(screen_t* const, char*, char*, int, int, char*);

screen_t* default_screen();
screen_t* micro();

void init_screen();
screen_t* get_screen(int);

int get_num_screens();

void list_screens();

char* gen_header(const screen_t* const s, int show_user,
                 int timestamp, int epoch, int width);

void delete_screen(screen_t* s);
void delete_screens();


#endif  /* _SCREEN_H */
