#ifndef _PROCESS_H
#define _PROCESS_H

#include <stdint.h>
#include <sys/time.h>

#define TXT_LEN 120

/* Main structure describing a thread */
struct process {
  pid_t    tid;          /* thread ID */
  pid_t    pid;          /* process ID. For owning process, tip == pid */
  int      num_threads;  /* number of threads in brotherhood */
  char*    name;         /* name of process */
  double   cpu_percent;  /* %CPU as displayed by top */
  
  struct timeval timestamp;         /* timestamp of last update */
  unsigned long prev_cpu_time;
  int       fd1, fd2, fd3, fd4;     /* file handles */
  uint64_t  val1, val2, val3, val4; /* values read from counters */
  uint64_t  prev_val1, prev_val2, prev_val3, prev_val4; /* previous iteration */
  char* txt;
};


/* List of processes/threads */
struct process_list {
  int  num_tids;
  int  num_alloc;
  struct process* processes;
};


struct process_list* init_proc_list();
void done_proc_list(struct process_list*);
void update_proc_list(struct process_list*);
void accumulate_stats(struct process_list*);
void reset_values(struct process_list*);


#endif  /* _PROCESS_H */
