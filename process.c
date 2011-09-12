#include <assert.h>
#include <dirent.h>
#include <inttypes.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "hash.h"
#include "options.h"
#include "pmc.h"
#include "process.h"
#include "screen.h"
#include "utils.h"

extern int debug;

static int num_files = 0;
static int num_files_limit = 0;

static int   clk_tck;


/*
 * Build the (empty) list of processes/threads.
 */
struct process_list* init_proc_list()
{
  char  name[100] = { 0 };  /* needs to fit the name /proc/xxxx/limits */
  char  line[100];
  FILE* f;

  clk_tck = sysconf(_SC_CLK_TCK);

  struct process_list* l = malloc(sizeof(struct process_list));
  l->num_alloc = 20;
  l->processes = malloc(l->num_alloc * sizeof(struct process));
  l->proc_ptrs = malloc(l->num_alloc * sizeof(struct process*));
  l->num_tids = 0;

  hash_init();

  num_files_limit = 0;
  snprintf(name, sizeof(name) - 1, "/proc/%d/limits", getpid());
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

  assert(list && list->processes && list->proc_ptrs);
  p = list->processes;
  for(i=0; i < list->num_tids; i++)
    done_proc(&p[i]);

  free(p);
  free(list->proc_ptrs);
  free(list);
  hash_fini();
}


void new_processes(struct process_list* const list,
                   const screen_t* const screen,
                   const struct option* const options)
{
  static int most_recent_pid = 0;
  struct dirent* pid_dirent;
  DIR*           pid_dir;
  int            num_tids, val;
  struct STRUCT_NAME events = {0, };
  FILE*          f;
  uid_t          my_uid = -1;

  const int cpu = -1;
  const int grp = -1;
  const int flags = 0;

  f = fopen("/proc/loadavg", "r");
  fscanf(f, "%*f %*f %*f %*d/%*d %d", &val);
  fclose(f);
  if (val == most_recent_pid)  /* no new process since last time */
    return;

  most_recent_pid = val;

  num_tids = list->num_tids;

  events.disabled = 0;
  events.pinned = 1;
  events.exclude_hv = 1;
  /* events.exclude_idle = 1; ?? */
  if (options->show_kernel == 0)
    events.exclude_kernel = 1;

  /* check all directories of /proc */
  pid_dir = opendir("/proc");
  while ((pid_dirent = readdir(pid_dir))) {
    int   uid, pid, num_threads;
    char  name[50] = { 0 }; /* needs to fit /proc/xxxx/{status,cmdline} */
    FILE* f;
    char  line[100]; /* line of /proc/xxxx/status */
    char  proc_name[100];

    if (pid_dirent->d_type != DT_DIR)  /* not a directory */
      continue;

    if ((pid = atoi(pid_dirent->d_name)) == 0)  /* not a number */
      continue;

    snprintf(name, sizeof(name) - 1, "/proc/%d/status", pid);
    f = fopen(name, "r");
    if (!f)
      continue;

    /* collect basic information about process */
    while (fgets(line, sizeof(line), f)) {
      /* read the name now, since we will encounter it before Uid anyway */
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
    my_uid = options->euid;
    if (((options->watch_uid != -1) && (uid == options->watch_uid)) ||
        ((options->watch_uid == -1) &&
         (((my_uid != 0) && (uid == my_uid)) ||  /* not root, monitor mine */
          ((my_uid == 0) && (uid != 0)))))  /* I am root, monitor all others */
    {
      DIR* thr_dir;
      struct dirent* thr_dirent;
      int  tid;
      char task_name[50] = { 0 };

      snprintf(task_name, sizeof(task_name) - 1, "/proc/%d/task", pid);
      thr_dir = opendir(task_name);
      if (!thr_dir) {
        perror("opendir");
        fprintf(stderr, "Cannot open '%s': '%s'\n", task_name, proc_name);
        continue;
      }
      /* Iterate over all threads in the process */
      while ((thr_dirent = readdir(thr_dir))) {
        int   zz, fail;
        char* cmdline = NULL;
        struct process* ptr;
        struct passwd*  passwd;
        struct process* p;
        tid = atoi(thr_dirent->d_name);
        if (tid == 0)
          continue;

        ptr = hash_get(tid);
        if (ptr)  /* already known */
          continue;


        /* We have a new thread. */

        /* reallocate space as needed. */
        if (num_tids == list->num_alloc) {
          struct process* old = list->processes;
          list->num_alloc += 20;
          list->processes = realloc(list->processes,
                                    list->num_alloc*sizeof(struct process));
          list->proc_ptrs = realloc(list->proc_ptrs,
                                    list->num_alloc*sizeof(struct process));
          if (list->processes != old) {  /* things have moved */
            int i;
            hash_fini();
            hash_fillin(list);
            for(i = 0; i < list->num_tids; i++)
              list->proc_ptrs[i] = &list->processes[i];
          }
        }

        p = list->processes;
        list->proc_ptrs[num_tids] = &p[num_tids];
        p[num_tids].tid = tid;
        p[num_tids].pid = pid;
        p[num_tids].proc_id = -1;
        p[num_tids].dead = 0;
        p[num_tids].attention = 0;
        p[num_tids].u.d = 0.0;

        hash_add(tid, &p[num_tids]);

        passwd = getpwuid(uid);
        if (passwd)
          p[num_tids].username = strdup(passwd->pw_name);
        else
          p[num_tids].username = NULL;

        p[num_tids].num_threads = num_threads;

        /* get process' command line */
        snprintf(name, sizeof(name) - 1, "/proc/%d/cmdline", pid);
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

        for(zz = 0; zz < p[num_tids].num_events; zz++)
          p[num_tids].prev_values[zz] = 0;

        p[num_tids].txt = malloc(TXT_LEN * sizeof(char));

        fail = 0;

        for(zz = 0; zz < p[num_tids].num_events; zz++) {
          int fd;

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
    }
  }
  closedir(pid_dir);
}


/*
 * Update all processes in the list with newly collected statistics.
 * Return the number of dead processes.
 */
int update_proc_list(struct process_list* const list,
                     const screen_t* const screen,
                     const struct option* const options)
{
  struct process* p;
  int    i, num_tids;
  int    num_dead = 0;

  assert(screen);
  assert(list && list->processes && list->proc_ptrs);
  p = list->processes;
  num_tids = list->num_tids;

  /* add newly created processes/threads */
  new_processes(list, screen, options);

  /* update statistics */
  for(i=0; i < list->num_tids; ++i) {
    struct process* proc = &list->processes[i];
    FILE*     fstat;
    char      sub_task_name[100] = { 0 };
    double    elapsed;
    unsigned long   utime = 0, stime = 0;
    unsigned long   prev_cpu_time, curr_cpu_time;
    int             proc_id, zz;
    struct timeval  now;

    if (proc->dead)
      continue;

    /* Compute %CPU */
    snprintf(sub_task_name, sizeof(sub_task_name) - 1,
             "/proc/%d/task/%d/stat", proc->pid, proc->tid);
    fstat = fopen(sub_task_name, "r");

    if (!fstat) {  /* this task disappeared */
      num_dead++;
      proc->dead = 1;  /* mark dead */
      for(zz=0; zz < proc->num_events; ++zz) {
        if (proc->fd[zz] != -1) {
          close(proc->fd[zz]);
          num_files--;
          proc->fd[zz] = -1;
        }
      }
      continue;
    }

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
    elapsed = (now.tv_sec - proc->timestamp.tv_sec) +
      (now.tv_usec - proc->timestamp.tv_usec)/1000000.0;
    elapsed *= clk_tck;

    proc->timestamp = now;

    prev_cpu_time = proc->prev_cpu_time_s + proc->prev_cpu_time_u;
    curr_cpu_time = stime + utime;
    proc->cpu_percent = 100.0*(curr_cpu_time - prev_cpu_time)/elapsed;
    proc->cpu_percent_s = 100.0*(stime - proc->prev_cpu_time_s)/elapsed;
    proc->cpu_percent_u = 100.0*(utime - proc->prev_cpu_time_u)/elapsed;

    proc->prev_cpu_time_s = stime;
    proc->prev_cpu_time_u = utime;

    proc->proc_id = proc_id;
    /* Backup previous value of counters */
    for(zz = 0; zz < proc->num_events; zz++)
      proc->prev_values[zz] = proc->values[zz];

    /* Read performance counters */
    for(zz = 0; zz < proc->num_events; zz++) {
      uint64_t value = 0;
      int r;
      /* When fd is -1, the syscall failed on that counter */
      if (proc->fd[zz] != -1) {
        r = read(proc->fd[zz], &value, sizeof(value));
        if (r == sizeof(value))
          proc->values[zz] = value;
        else
          proc->values[zz] = 0;
      }
      else  /* no fd, use marker */
        proc->values[zz] = 0xffffffff;
    }
    continue;
  }

  return num_dead;
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

  /* update pointers */
  for(src = 0; src < list->num_tids; src++)
    list->proc_ptrs[src] = &list->processes[src];

  hash_fini();
  hash_fillin(list);
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
      struct process* owner;

      if (p[i].dead)
        continue;

      /* find the owner */
      owner = hash_get(p[i].pid);
      assert(owner);

      /* accumulate in owner process */
      owner->cpu_percent += p[i].cpu_percent;
      for(zz = 0; zz < p[i].num_events; zz++) {
        /* as soon as one thread has invalid value, skip entire process. */
        if (p[i].values[zz] == 0xffffffff) {
          owner->values[zz] = 0xffffffff;
          break;
        }
        owner->values[zz] += p[i].values[zz];
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
