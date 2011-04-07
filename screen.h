#ifndef _SCREEN_H
#define _SCREEN_H

#include <inttypes.h>


typedef struct {
  uint32_t  type;
  uint64_t  config;
} counter_t;


typedef struct {
  int   id;
  char* name;
  int   num_counters;
  counter_t* counters;
} screen_t;


#endif  /* _SCREEN_H */
