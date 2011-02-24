#include <assert.h>
#include <dirent.h>
#include <inttypes.h>

#ifdef KERNEL31
#include <linux/perf_counter.h>
#else
#include <linux/perf_event.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "pmc.h"
#include "process.h"

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
    free(p[i].name);
    free(p[i].txt);
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
void update_proc_list(struct process_list* list)
{
  int i;
  struct dirent* pid_dirent;
  DIR* pid_dir;
  uid_t  my_uid;
  int    cpu, grp, flags;
  int    clk_tck;
  struct process* p;
  struct STRUCT_NAME events = {0, };

  assert(list && list->processes);
  p = list->processes;

  clk_tck = sysconf(_SC_CLK_TCK);

  /* remove dead threads */
  for(i=0; i < list->num_tids; ++i) {
    char name[50]; /* needs to fit the name /proc/xxxxx/task/xxxxx/status */
    sprintf(name, "/proc/%d/task/%d/status", p[i].pid, p[i].tid);
    if (access(name, F_OK) == -1) {
      p[i].tid = 0;  /* mark dead */
      p[i].pid = 0;  /* mark dead */
      if (p[i].fd1 != -1)
        close(p[i].fd1);
      if (p[i].fd2 != -1)
        close(p[i].fd2);
      if (p[i].fd3 != -1)
        close(p[i].fd3);
      if (p[i].fd4 != -1)
        close(p[i].fd4);
      p[i].fd1 = p[i].fd2 = p[i].fd3 = p[i].fd4 = -1;
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

    if (uid == my_uid) {  /* unless I am root?? */
      int fd1, fd2, fd3, fd4;
      int  tid;
      DIR* thr_dir;
      struct dirent* thr_dirent;
      char task_name[50];
      /* printf("PID %d has %d threads\n", pid, num_threads); */
      sprintf(task_name, "/proc/%d/task", pid);
      thr_dir = opendir(task_name);

      /* Iterate over all threads in the process */
      while ((thr_dirent = readdir(thr_dir))) {
        int pos;
        tid = atoi(thr_dirent->d_name);

        if (tid == 0)
          continue;

        pos = pos_in_list(list, tid);
        if (pos != -1) {  /* already known */
          int       r1, r2, r3, r4;
          uint64_t  insns, cycles, misses, brmisses;
          FILE*     fstat;
          char      task_name[100];
          double    elapsed;
          unsigned long   utime = 0, stime = 0;
          struct timeval  now;
          struct process* ptr = &(list->processes[pos]);

          /* Compute %CPU */
          sprintf(task_name, "/proc/%d/task/%d/stat", pid, tid);
          fstat = fopen(task_name, "r");
          if (fstat) {
            (void)fscanf(fstat, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", &utime, &stime);
            fclose(fstat);
          }
          gettimeofday(&now, NULL);
          elapsed = (now.tv_sec - ptr->timestamp.tv_sec) +
                    (now.tv_usec - ptr->timestamp.tv_usec)/1000000.0; 

          ptr->timestamp = now;
          ptr->cpu_percent = 100.0*(utime + stime - ptr->prev_cpu_time)/clk_tck/elapsed;
          ptr->prev_cpu_time = utime + stime;


          /* Read performance counters */
          ptr->prev_val1 = ptr->val1;
          ptr->prev_val2 = ptr->val2;
          ptr->prev_val3 = ptr->val3;
          ptr->prev_val4 = ptr->val4;

          r1 = read(ptr->fd1, &cycles, sizeof(cycles));
          r2 = read(ptr->fd2, &insns, sizeof(insns));
          r3 = read(ptr->fd3, &misses, sizeof(misses));
          r4 = read(ptr->fd4, &brmisses, sizeof(brmisses));

          if (r1 == sizeof(cycles))
            ptr->val1 = cycles;
          else
            ptr->val1 = 0;

          if (r2 == sizeof(insns))
            ptr->val2 = insns;
          else
            ptr->val2 = 0;

          if (r3 == sizeof(misses))
            ptr->val3 = misses;
          else
            ptr->val3 = 0;

          if (r4 == sizeof(brmisses))
            ptr->val4 = brmisses;
          else
            ptr->val4 = 0;

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
        p[list->num_tids].prev_val1 = 0;
        p[list->num_tids].prev_val2 = 0;
        p[list->num_tids].prev_val3 = 0;
        p[list->num_tids].prev_val4 = 0;
        p[list->num_tids].txt = malloc(TXT_LEN * sizeof(char));

        /* First counter: cycles */
        events.type = PERF_TYPE_HARDWARE;
        events.config = PERF_COUNT_HW_CPU_CYCLES;
        fd1 = sys_perf_counter_open(&events, tid, cpu, grp, flags);
        p[list->num_tids].fd1 = fd1;
        p[list->num_tids].val1 = 0;

        /* Second counter: instructions */
        events.type = PERF_TYPE_HARDWARE;
        events.config = PERF_COUNT_HW_INSTRUCTIONS;
        fd2 = sys_perf_counter_open(&events, tid, cpu, grp, flags);
        p[list->num_tids].fd2 = fd2;
        p[list->num_tids].val2 = 0;

        /* Third counter: cache misses */
        events.type = PERF_TYPE_HARDWARE;
        events.config = PERF_COUNT_HW_CACHE_MISSES;
        fd3 = sys_perf_counter_open(&events, tid, cpu, grp, flags);
        p[list->num_tids].fd3 = fd3;
        p[list->num_tids].val3 = 0;

        /* Fourth counter: page faults */
        events.type = PERF_TYPE_HARDWARE;
        events.config = PERF_COUNT_HW_BRANCH_MISSES;
        fd4 = sys_perf_counter_open(&events, tid, cpu, grp, flags);
        p[list->num_tids].fd4 = fd4;
        p[list->num_tids].val4 = 0;

        if ((fd1 != -1) && (fd2 != -1) && (fd3 != -1) && (fd4 != -1)) {
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
              p[i].fd1, p[i].fd2, p[i].fd3, p[i].fd4,
              p[i].val1, p[i].val2, p[i].val3, p[i].val4);
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
  int i;
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
      p[pos].val1 += p[i].val1;
      p[pos].val2 += p[i].val2;
      p[pos].val3 += p[i].val3;
      p[pos].val4 += p[i].val4;
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
  int i;
  struct process* p;

  p = list->processes;
  for(i=0; i < list->num_tids; ++i) {
    /* only consider 'main' processes (not threads) */
    if (p[i].pid == p[i].tid) {
      p[i].cpu_percent = 0;
      p[i].val1 = 0;
      p[i].val2 = 0;
      p[i].val3 = 0;
      p[i].val4 = 0;
    }
  }
}
