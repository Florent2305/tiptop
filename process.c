#include <assert.h>
#include <dirent.h>
#include <inttypes.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "options.h"
#include "pmc.h"
#include "process.h"
#include "screen.h"
#include "utils.h"

extern int debug;

static int num_files = 0;
static int num_files_limit = 0;

static uid_t my_uid = -1;
static int   clk_tck;


/*
 * Build the (empty) list of processes/threads.
 */
struct process_list* init_proc_list()
{
  char  name[100];  /* needs to fit the name /proc/xxxx/limits */
  char  line[100];
  FILE* f;

  my_uid = geteuid();
  clk_tck = sysconf(_SC_CLK_TCK);

  struct process_list* l = malloc(sizeof(struct process_list));
  l->num_alloc = 20;
  l->processes = malloc(l->num_alloc*sizeof(struct process));
  l->num_tids = 0;

  num_files_limit = 0;
  sprintf(name, "/proc/%d/limits", getpid());
  f = fopen(name, "r");
  if (f) {
    while (fgets(line, 100, f)) {
      int n;
      n = sscanf(line, "Max open files %d", &num_files_limit);
      if (n)
        break;
    }
    fclose(f);
  }

  num_files_limit -= 10; /* keep some slack */
  if (num_files_limit == 0)  /* something went wrong */
    num_files_limit = 200;  /* reasonable default? */
  num_files = 0;
  return l;
}


/* Free memory for all fields of the process. */
static void done_proc(struct process* const p)
{
  int val_idx;
  
  if (p->cmdline)
    free(p->cmdline);
  free(p->name);
  free(p->txt);
  if (p->username)
    free(p->username);

  for(val_idx=0; val_idx < p->num_events; val_idx++) {
    if (p->fd[val_idx] != -1) {
      close(p->fd[val_idx]);
      num_files--;
    }
  }
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
    done_proc(&p[i]);
  }
  free(p);
  free(list);
}


/* 
 * Return the index in the list of the thread 'tid'.
 * -1 if not present.
 */
static int pos_in_list(const struct process_list* const list, pid_t tid)
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
 * Return 1 if processes have died.
 */
int update_proc_list(struct process_list* const list,
                     const screen_t* const screen,
                     const struct option* const options)
{
  int i;
  struct dirent* pid_dirent;
  DIR* pid_dir;
  int    cpu, grp, flags, num_tids;
  struct process* p;
  struct STRUCT_NAME events = {0, };
  int    ret_val = 0;

  assert(screen);
  assert(list && list->processes);
  p = list->processes;

  /* mark dead tasks */
  num_tids = list->num_tids;
  for(i=0; i < list->num_tids; ++i) {
    char name[50]; /* needs to fit the name /proc/xxxxx/task/yyyyy/status */
    int  zz;

    sprintf(name, "/proc/%d/task/%d/status", p[i].pid, p[i].tid);
    if (access(name, F_OK) == -1) {
      ret_val = 1;  /* at least one dead */
      p[i].dead = 1;  /* mark dead */
      for(zz=0; zz < p[i].num_events; ++zz) {
        if (p[i].fd[zz] != -1) {
          close(p[i].fd[zz]);
          num_files--;
          p[i].fd[zz] = -1;
        }
      }
    }
  }


  /* update statistics, and add newly created processes/threads */

  cpu = -1;  /* CPU to monitor, -1 = per thread */
  grp = -1;
  flags = 0;

  events.disabled = 0;
  events.pinned = 1;
  events.exclude_hv = 1;
  /* events.exclude_idle = 1; ?? */
  if (options->show_kernel == 0)
    events.exclude_kernel = 1;

  /* check all directories of /proc */
  pid_dir = opendir("/proc");
  while ((pid_dirent = readdir(pid_dir))) {
    char  name[50]; /* needs to fit the name /proc/xxxx/{status,cmdline} */
    char  proc_name[1000];
    char* cmdline = NULL;
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
    if (((options->watch_uid != -1) && (uid == options->watch_uid)) ||
        ((options->watch_uid == -1) &&
         (((my_uid != 0) && (uid == my_uid)) ||  /* not root, monitor mine */
          ((my_uid == 0) && (uid != 0)))))  /* I am root, monitor all others */
    { 
      int  fd;
      int  tid;
      int  fail;
      DIR* thr_dir;
      struct dirent* thr_dirent;
      char task_name[50];

      sprintf(task_name, "/proc/%d/task", pid);
      thr_dir = opendir(task_name);
      if (!thr_dir) {
        perror("opendir");
        fprintf(stderr, "Cannot open '%s': '%s'\n", task_name, proc_name);
        goto skip;
      }

      /* Iterate over all threads in the process */
      while ((thr_dirent = readdir(thr_dir))) {
        int pos, zz;
        struct passwd* passwd;

        tid = atoi(thr_dirent->d_name);
        if (tid == 0)
          continue;

        pos = pos_in_list(list, tid);
        if (pos != -1) {  /* already known */
          FILE*     fstat;
          char      sub_task_name[100];
          double    elapsed;
          unsigned long   utime = 0, stime = 0;
          unsigned long   prev_cpu_time, curr_cpu_time;
          int             proc_id;
          struct timeval  now;
          struct process* ptr = &(list->processes[pos]);

          /* Compute %CPU */
          sprintf(sub_task_name, "/proc/%d/task/%d/stat", pid, tid);
          fstat = fopen(sub_task_name, "r");
          if (fstat) {
            int n;
            n = fscanf(fstat,
                 "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",
                       &utime, &stime);
            if (n != 2) {
              fprintf(stderr, "Cannot read from '%s'\n", sub_task_name);
              exit(EXIT_FAILURE);
            }
            /* get processor ID */
            n = fscanf(fstat,
                       "%*d %*d %*d %*d %*d %*d %*d %*u %*d %*u %*u %*u %*u "
                       "%*u %*u %*u %*u %*u %*u %*u %*u %*u %*d %d",
                       &proc_id);
            if (n != 1)
              proc_id = -1;
            fclose(fstat);
          }
          gettimeofday(&now, NULL);
          elapsed = (now.tv_sec - ptr->timestamp.tv_sec) +
                    (now.tv_usec - ptr->timestamp.tv_usec)/1000000.0;
          elapsed *= clk_tck;

          ptr->timestamp = now;

          prev_cpu_time = ptr->prev_cpu_time_s + ptr->prev_cpu_time_u;
          curr_cpu_time = stime + utime;
          ptr->cpu_percent = 100.0*(curr_cpu_time - prev_cpu_time)/elapsed;
          ptr->cpu_percent_s = 100.0*(stime - ptr->prev_cpu_time_s)/elapsed;
          ptr->cpu_percent_u = 100.0*(utime - ptr->prev_cpu_time_u)/elapsed;

          ptr->prev_cpu_time_s = stime;
          ptr->prev_cpu_time_u = utime;

          ptr->proc_id = proc_id;
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
        if (num_tids == list->num_alloc) {
          list->num_alloc += 10;
          list->processes = realloc(list->processes,
                                    list->num_alloc*sizeof(struct process));
        }

        p = list->processes;
        p[num_tids].tid = tid;
        p[num_tids].pid = pid;
        p[num_tids].proc_id = -1;
        p[num_tids].dead = 0;
        p[num_tids].attention = 0;

        passwd = getpwuid(uid);
        if (passwd) {
          p[num_tids].username = strdup(passwd->pw_name);
        }
        else
          p[num_tids].username = NULL;

        p[num_tids].num_threads = num_threads;

        /* get process' command line */
        sprintf(name, "/proc/%d/cmdline", pid);
        f = fopen(name, "r");
        if (f) {
          char  buffer[100];
          char* res;

          memset(buffer, 0, sizeof(buffer));
          res = fgets(buffer, sizeof(buffer), f);
          if (res && res[0]) {
            int j;
            for(j=0; j < sizeof(buffer)-1; j++) {
              if (buffer[j] == '\0') {
                if (buffer[j+1] == '\0')  /* two zeroes in a row, stop */
                  break;
                else
                  buffer[j] = ' '; /* only one, a separator, it becomes ' ' */
              }
            }
            
            cmdline = strdup(res);
          }
          fclose(f);
        }
        if (!cmdline)
          cmdline = strdup("[null]");

        p[num_tids].cmdline = cmdline;
        p[num_tids].name = strdup(proc_name);
        p[num_tids].timestamp.tv_sec = 0;
        p[num_tids].timestamp.tv_usec = 0;
        p[num_tids].prev_cpu_time_s = 0;
        p[num_tids].prev_cpu_time_u = 0;
        p[num_tids].cpu_percent = 0.0;
        p[num_tids].cpu_percent_s = 0.0;
        p[num_tids].cpu_percent_u = 0.0;

        /* Get number of counters from screen */
        p[num_tids].num_events = screen->num_counters;

        for(zz = 0; zz < p[num_tids].num_events; zz++) {
          p[num_tids].prev_values[zz] = 0;
        }
        p[num_tids].txt = malloc(TXT_LEN * sizeof(char));

        fail = 0;

        for(zz = 0; zz < p[num_tids].num_events; zz++) {
          events.type = screen->counters[zz].type;  /* eg PERF_TYPE_HARDWARE */
          events.config = screen->counters[zz].config;

          if (num_files < num_files_limit)
            fd = sys_perf_counter_open(&events, tid, cpu, grp, flags);
          else
            fd = -1;

          if (fd == -1)
            fail++;
          else
            num_files++;
          p[num_tids].fd[zz] = fd;
          p[num_tids].values[zz] = 0;
        }

        if (fail) {
          /* at least one counter failed, mark it */
          p[num_tids].attention = 1;
        }

#if 0
        if (fail != p[num_tids].num_events) {
          /* at least one counter succeeded, insert the thread in list */
          list->num_tids++;
          num_tids++;
        }
#endif
        list->num_tids++;  /* insert in any case */
        num_tids++;
      }
      closedir(thr_dir);

    skip: ;
    }
  }

  closedir(pid_dir);
  return ret_val;
}


/* Scan list of processes and deallocates the dead ones, compacting
   the list. */
void compact_proc_list(struct process_list* const list)
{
  int dst, src, num_tids, num_dead;
  struct process* p;

  p = list->processes;
  num_tids = list->num_tids;
  dst = 0;
  num_dead = 0;
  for(src=0; src < num_tids; src++, dst++) {
    while ((src < num_tids) && (p[src].dead == 1)) {
      done_proc(&p[src]);
      src++;
      num_dead++;
    }
    if ((src != dst) && (src < num_tids)) {
      p[dst] = p[src];
    }
  }
  list->num_tids -= num_dead;
}


/*
 * When threads are not displayed, this function accumulates
 * per-thread statistics in the parent process (which is also a
 * thread).
 */
void accumulate_stats(const struct process_list* const list)
{
  int i, zz;
  struct process* p;

  p = list->processes;
  for(i=0; i < list->num_tids; ++i) {
    if (p[i].pid != p[i].tid) {
      int pos;

      if (p[i].dead)
        continue;

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
void reset_values(const struct process_list* const list)
{
  int i, zz;
  struct process* p;

  p = list->processes;
  for(i=0; i < list->num_tids; ++i) {
    if (p[i].dead)
      continue;
    /* only consider 'main' processes (not threads) */
    if (p[i].pid == p[i].tid) {
      p[i].cpu_percent = 0;
      for(zz = 0; zz < p[i].num_events; zz++) {
        p[i].values[zz] = 0;
      }
    }
  }
}
