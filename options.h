#ifndef _OPTIONS_H
#define _OPTIONS_H

#include <sys/types.h>


/* global state */
struct option {
  float  delay;
  float  cpu_threshold;  /* CPU activity below which a thread is considered inactive */
  int    max_iter;
  char*  only_name;
  int    only_pid;
  char*  watch_name;
  pid_t  watch_pid;
  int    watch_uid;
  int    batch : 1;
  int    debug : 1;
  int    help : 1;
  int    idle : 1;
  int    show_cmdline : 1;
  int    show_epoch : 1;
  int    show_kernel : 1;
  int    show_threads : 1;
  int    show_timestamp : 1;
  int    show_user : 1;
  int    sticky : 1;
};


void init_options(struct option* opt);
void parse_command_line(int argc, char* argv[], struct option* const,int*,int*);

#endif  /* _OPTIONS_H */
