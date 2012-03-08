/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2011 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#ifndef _OPTIONS_H
#define _OPTIONS_H

#include <stdio.h>
#include <sys/types.h>


/* global state */
struct option {
  int    spawn_pos;
  float  delay;
  float  cpu_threshold;  /* CPU activity below which a thread is considered inactive */
  int    max_iter;
  char*  only_name;
  int    only_pid;
  char*  watch_name;
  pid_t  watch_pid;
  int    watch_uid;
  
  uid_t  euid;  /* effective user ID of tiptop */
  FILE*  out;

  unsigned int    batch : 1;
  unsigned int    command_done : 1;
  unsigned int    debug : 1;
  unsigned int    help : 1;
  unsigned int    idle : 1;
  unsigned int    show_cmdline : 1;
  unsigned int    show_epoch : 1;
  unsigned int    show_kernel : 1;
  unsigned int    show_threads : 1;
  unsigned int    show_timestamp : 1;
  unsigned int    show_user : 1;
  unsigned int    sticky : 1;
};


void init_options(struct option* opt);
void parse_command_line(int argc, char* argv[], struct option* const,int*,int*);

#endif  /* _OPTIONS_H */
