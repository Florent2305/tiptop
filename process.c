#include <assert.h>
#include <dirent.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "pmc.h"
#include "process.h"
#include "screen.h"

extern int debug;


/*
 * Build the (empty) list of processes/threads.
 */
struct process_list* init_proc_list()
{
  struct process_list* l = malloc(sizeof(struct process_list));
  l->num_alloc = 20;
  l->processes = malloc(l->num_alloc*sizeof(struct process));
  l->num_tids = 0;
  return l;
}


/*
 * Deletes the list of processes. Free memory.
 */
void done_proc_list(struct process_list* list)
{
  int i;
  struct process* p;

  assert(list && list->processes);
  p = list->processes;
  for(i=0; i < list->num_tids; i++) {
    int val_idx;
    free(p[i].name);
    free(p[i].txt);
    for(val_idx=0; val_idx < p[i].num_events; val_idx++) {
      if (p[i].fd[val_idx] != -1) {
        close(p[i].fd[val_idx]);
      }
    }
  }
  free(p);
  free(list);
}


/* 
 * Return the index in the list of the thread 'tid'.
 * -1 if not present.
 */
static int pos_in_list(struct process_list* list, pid_t tid)
{
  int i;
  struct process* p;

  assert(list && list->processes);
  p = list->processes;
  for(i=0; i < list->num_tids; i++) {
    if (p[i].tid == tid)
      return i;
  }
  return -1;
}


/*
 * Update all processes in the list with newly collected statistics.
 *
 */
void update_proc_list(struct process_list* list, const screen_t* const screen)
{
  int i;
  struct dirent* pid_dirent;
  DIR* pid_dir;
  uid_t  my_uid;
  int    cpu, grp, flags;
  int    clk_tck;
  struct process* p;
  struct STRUCT_NAME events = {0, };

  assert(screen);
  assert(list && list->processes);
  p = list->processes;

  clk_tck = sysconf(_SC_CLK_TCK);

  /* remove dead threads */
  for(i=0; i < list->num_tids; ++i) {
    char name[50]; /* needs to fit the name /proc/xxxxx/task/xxxxx/status */
    int  zz;

    sprintf(name, "/proc/%d/task/%d/status", p[i].pid, p[i].tid);
    if (access(name, F_OK) == -1) {
      p[i].tid = 0;  /* mark dead */
      p[i].pid = 0;  /* mark dead */
      for(zz=0; zz < p[i].num_events; ++zz) {
        if (p[i].fd[zz] != -1) {
          close(p[i].fd[zz]);
          p[i].fd[zz] = -1;
        }
      }
    }
  }


  /* update statistics, and add newly created processes/threads */
  my_uid = getuid();
  cpu = -1;  /* CPU to monitor, -1 = per thread */
  grp = -1;
  flags = 0;

  events.disabled = 0;
  events.exclude_hv = 1;
  events.exclude_kernel = 1;

  /* check all directories of /proc */
  pid_dir = opendir("/proc");
  while ((pid_dirent = readdir(pid_dir))) {
    char  name[50]; /* needs to fit the name /proc/xxxx/status */
    char  proc_name[1000];
    char  line[100]; /* line of /proc/xxxx/status */
    int   uid;
    int   pid;
    int   num_threads;
    FILE* f;

    if (pid_dirent->d_type != DT_DIR)  /* not a directory */
      continue;

    if ((pid = atoi(pid_dirent->d_name)) == 0)  /* not a number */
      continue;

    sprintf(name, "/proc/%d/status", pid);
    f = fopen(name, "r");
    if (!f)
      continue;

    /* collect basic information about process */
    while (fgets(line, sizeof(line), f)) {
      if (strncmp(line, "Name:", 5) == 0) {
        sscanf(line, "%*s %s", proc_name);
      }
      if (strncmp(line, "Uid:", 4) == 0) {
        sscanf(line, "%*s %d", &uid);
      }
      if (strncmp(line, "Threads:", 8) == 0) {
        sscanf(line, "%*s %d", &num_threads);
      }
    }
    fclose(f);

    /* my process, or somebody else's process and I am root (skip
       root's processes because they are too many. */
    if ((uid == my_uid) || ((my_uid == 0) && (uid != 0))) {
      int  fd;
      int  tid;
      int  fail;
      DIR* thr_dir;
      struct dirent* thr_dirent;
      char task_name[50];
      /* printf("PID %d has %d threads\n", pid, num_threads); */
      sprintf(task_name, "/proc/%d/task", pid);
      thr_dir = opendir(task_name);

      /* Iterate over all threads in the process */
      while ((thr_dirent = readdir(thr_dir))) {
        int pos, zz;
        tid = atoi(thr_dirent->d_name);

        if (tid == 0)
          continue;

        pos = pos_in_list(list, tid);
        if (pos != -1) {  /* already known */
          FILE*     fstat;
          char      sub_task_name[100];
          double    elapsed;
          unsigned long   utime = 0, stime = 0;
          struct timeval  now;
          struct process* ptr = &(list->processes[pos]);

          /* Compute %CPU */
          sprintf(sub_task_name, "/proc/%d/task/%d/stat", pid, tid);
          fstat = fopen(sub_task_name, "r");
          if (fstat) {
            int n;
            n = fscanf(fstat, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", &utime, &stime);
            if (n != 2) {
              fprintf(stderr, "Cannot read from '%s'\n", sub_task_name);
              exit(EXIT_FAILURE);
            }
            fclose(fstat);
          }
          gettimeofday(&now, NULL);
          elapsed = (now.tv_sec - ptr->timestamp.tv_sec) +
                    (now.tv_usec - ptr->timestamp.tv_usec)/1000000.0; 

          ptr->timestamp = now;
          ptr->cpu_percent = 100.0*(utime + stime - ptr->prev_cpu_time)/clk_tck/elapsed;
          ptr->prev_cpu_time = utime + stime;


          /* Backup previous value of counters */
          for(zz = 0; zz < ptr->num_events; zz++) {
            ptr->prev_values[zz] = ptr->values[zz];
          }

          /* Read performance counters */
          for(zz = 0; zz < ptr->num_events; zz++) {
            uint64_t value = 0;
            int r;
            /* When fd is -1, the syscall failed on that counter */
            if (ptr->fd[zz] != -1) {
              r = read(ptr->fd[zz], &value, sizeof(value));
              if (r == sizeof(value))
                ptr->values[zz] = value;
              else
                ptr->values[zz] = 0;
            }
            else {
              /* no fd, use marker */
              ptr->values[zz] = 0xffffffff;
            }
          }

          continue;
        }

        /* We have a new thread. Reallocate space as needed. */
        if (list->num_tids == list->num_alloc) {
          list->num_alloc += 10;
          list->processes = realloc(list->processes,
                                    list->num_alloc*sizeof(struct process));
        }

        p = list->processes;
        p[list->num_tids].tid = tid;
        p[list->num_tids].pid = pid;
        p[list->num_tids].num_threads = num_threads;
        p[list->num_tids].name = strdup(proc_name);
        p[list->num_tids].timestamp.tv_sec = 0;
        p[list->num_tids].timestamp.tv_usec = 0;
        p[list->num_tids].prev_cpu_time = 0;
        p[list->num_tids].cpu_percent = 0.0;

        /* Get number of counters from screen */
        p[list->num_tids].num_events = screen->num_counters;

        for(zz = 0; zz < p[list->num_tids].num_events; zz++) {
          p[list->num_tids].prev_values[zz] = 0;
        }
        p[list->num_tids].txt = malloc(TXT_LEN * sizeof(char));

        fail = 0;

        for(zz = 0; zz < p[list->num_tids].num_events; zz++) {
          events.type = screen->counters[zz].type;  /* eg PERF_TYPE_HARDWARE */
          events.config = screen->counters[zz].config;
          fd = sys_perf_counter_open(&events, tid, cpu, grp, flags);
          if (fd == -1)
            fail++;
          p[list->num_tids].fd[zz] = fd;
          p[list->num_tids].values[zz] = 0;
        }

        if (fail != p[list->num_tids].num_events) {
          /* at least one counter succeeded, insert the thread in list */
          list->num_tids++;
        }
      }
      closedir(thr_dir);
    }
  }
  closedir(pid_dir);


  if (debug) {
    FILE* debug = fopen("debug", "w");
    p = list->processes;
    fprintf(debug, "num PIDs: %d\n", list->num_tids);
    for(i=0; i < list->num_tids; ++i) {
      fprintf(debug, "[%d] %5d-%5d %-15s  %d %d %d %d - %"PRIu64"  %"PRIu64"  %"PRIu64"  %"PRIu64" \n",
              i,
              p[i].pid, p[i].tid, p[i].name,
              p[i].fd[0], p[i].fd[1], p[i].fd[2], p[i].fd[3],
              p[i].values[0], p[i].values[1], p[i].values[2], p[i].values[3]);
    }
    fprintf(debug, "----------\n");
    fclose(debug);
  }
}


/*
 * When threads are not displayed, this function accumulates
 * per-thread statistics in the parent process (which is also a
 * thread).
 */
void accumulate_stats(struct process_list* list)
{
  int i, zz;
  struct process* p;

  p = list->processes;
  for(i=0; i < list->num_tids; ++i) {
    if (p[i].pid != p[i].tid) {
      int pos;
      /* find the owner */
      pos = pos_in_list(list, p[i].pid);
      assert(pos != -1);

      /* accumulate in owner process */
      p[pos].cpu_percent += p[i].cpu_percent;
      for(zz = 0; zz < p[i].num_events; zz++) {
        /* as soon as one thread has invalid value, skip entire process. */
        if (p[i].values[zz] == 0xffffffff) {
          p[pos].values[zz] = 0xffffffff;
          break;
        }
        p[pos].values[zz] += p[i].values[zz];
      }
    }
  }
}


/*
 * When switching back from show_threads to no_thread, we need to
 * reset the statistics of the parent thread, because they contain
 * accumulated values, much higher than per-thread value. Failing to
 * do so results in transient erratic displayed values.
 */
void reset_values(struct process_list* list)
{
  int i, zz;
  struct process* p;

  p = list->processes;
  for(i=0; i < list->num_tids; ++i) {
    /* only consider 'main' processes (not threads) */
    if (p[i].pid == p[i].tid) {
      p[i].cpu_percent = 0;
      for(zz = 0; zz < p[i].num_events; zz++) {
        p[i].values[zz] = 0;
      }
    }
  }
}
