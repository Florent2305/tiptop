#ifndef _PROCESS_H
#define _PROCESS_H

#include <stdint.h>
#include <sys/time.h>

#include "screen.h"


#define MAX_EVENTS 16
#define TXT_LEN 120

/* Main structure describing a thread */
struct process {
  pid_t    tid;           /* thread ID */
  pid_t    pid;           /* process ID. For owning process, tip == pid */
  int      proc_id;
  char*    username;
  int      num_threads;   /* number of threads in brotherhood */
  char*    cmdline;       /* command line */
  char*    name;          /* name of process */
  double   cpu_percent;   /* %CPU as displayed by top */
  double   cpu_percent_s; /* %CPU system */
  double   cpu_percent_u; /* %CPU user */
  
  struct timeval timestamp;         /* timestamp of last update */
  unsigned long prev_cpu_time_s;    /* system */
  unsigned long prev_cpu_time_u;    /* user */
  int       num_events;
  int       fd[MAX_EVENTS];           /* file handles */
  uint64_t  values[MAX_EVENTS];       /* values read from counters */
  uint64_t  prev_values[MAX_EVENTS];  /* previous iteration */
  char* txt;

  int   attention;
};


/* List of processes/threads */
struct process_list {
  int  num_tids;
  int  num_alloc;
  struct process* processes;
};


struct process_list* init_proc_list();
void done_proc_list(struct process_list*);
int  update_proc_list(struct process_list* const, const screen_t* const, int);
void compact_proc_list(struct process_list* const);
void accumulate_stats(const struct process_list* const);
void reset_values(const struct process_list* const);


#endif  /* _PROCESS_H */
