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
  l->processes = NULL;
  l->num_alloc = 20;
  l->proc_ptrs = malloc(l->num_alloc * sizeof(struct process*));
  l->num_tids = 0;
  l->most_recent_pid = 0;

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
  struct process* p;

  assert(list && list->proc_ptrs);
  p = list->processes;
  while (p) {
    struct process* old = p;
    done_proc(p);
    p = p->next;
    free(old);
  }

  free(list->proc_ptrs);
  free(list);
  hash_fini();
}


void new_processes(struct process_list* const list,
                   const screen_t* const screen,
                   const struct option* const options)
{
  struct dirent* pid_dirent;
  DIR*           pid_dir;
  int            num_tids, val;
  struct STRUCT_NAME events = {0, };
  FILE*          f;
  uid_t          my_uid = -1;

  const int cpu = -1;
  const int grp = -1;
  const int flags = 0;

  /* To avoid scanning the entire /proc directory, we first check if
     any process has been created since last time. /proc/loadavg
     contains the PID of the most recent process. We compare with our
     own most recent. */
  f = fopen("/proc/loadavg", "r");
  fscanf(f, "%*f %*f %*f %*d/%*d %d", &val);
  fclose(f);
  if (val == list->most_recent_pid)  /* no new process since last time */
    return;

  list->most_recent_pid = val;

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
    int   uid, pid, num_threads, req_info;
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
    req_info = 3;  /* need 3 pieces of information */
    while (fgets(line, sizeof(line), f) && req_info) {
      /* read the name now, since we will encounter it before Uid anyway */
      if (strncmp(line, "Name:", 5) == 0) {
        sscanf(line, "%*s %s", proc_name);
        req_info--;
      }
      if (strncmp(line, "Uid:", 4) == 0) {
        sscanf(line, "%*s %d", &uid);
        req_info--;
      }
      if (strncmp(line, "Threads:", 8) == 0) {
        sscanf(line, "%*s %d", &num_threads);
        req_info--;
      }
    }
    fclose(f);
    assert(req_info == 0);

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
      if (!thr_dir)  /* died just now? Will be marked dead at next iteration. */
        continue;

      /* Iterate over all threads in the process */
      while ((thr_dirent = readdir(thr_dir))) {
        int   zz, fail;
        char* cmdline = NULL;
        struct process* ptr;
        struct passwd*  passwd;

        tid = atoi(thr_dirent->d_name);
        if (tid == 0)
          continue;

        ptr = hash_get(tid);
        if (ptr)  /* already known */
          continue;


        /* We have a new thread. */

        /* allocate memory */
        ptr = malloc(sizeof(struct process));

        /* insert into list of processes */
        ptr->next = list->processes;
        list->processes = ptr;

        /* update helper data structures */
        if (num_tids == list->num_alloc) {
          list->num_alloc += 20;
          list->proc_ptrs = realloc(list->proc_ptrs,
                                    list->num_alloc*sizeof(struct process));
        }
        list->proc_ptrs[num_tids] = ptr;
        hash_add(tid, ptr);

        /* fill in information for new process */
        ptr->tid = tid;
        ptr->pid = pid;
        ptr->proc_id = -1;
        ptr->dead = 0;
#if 0
        ptr->attention = 0;
#endif
        ptr->u.d = 0.0;

        passwd = getpwuid(uid);
        if (passwd)
          ptr->username = strdup(passwd->pw_name);
        else
          ptr->username = NULL;

        ptr->num_threads = num_threads;

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

        ptr->cmdline = cmdline;
        ptr->name = strdup(proc_name);
        ptr->timestamp.tv_sec = 0;
        ptr->timestamp.tv_usec = 0;
        ptr->prev_cpu_time_s = 0;
        ptr->prev_cpu_time_u = 0;
        ptr->cpu_percent = 0.0;
        ptr->cpu_percent_s = 0.0;
        ptr->cpu_percent_u = 0.0;

        /* Get number of counters from screen */
        ptr->num_events = screen->num_counters;

        for(zz = 0; zz < ptr->num_events; zz++)
          ptr->prev_values[zz] = 0;

        ptr->txt = malloc(TXT_LEN * sizeof(char));

        fail = 0;
        for(zz = 0; zz < ptr->num_events; zz++) {
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
          ptr->fd[zz] = fd;
          ptr->values[zz] = 0;
        }

#if 0
        if (fail) {
          /* at least one counter failed, mark it */
          ptr->attention = 1;
        }

        if (fail != ptr->num_events) {
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
  struct process* proc;
  int    num_tids;
  int    num_dead = 0;

  assert(screen);
  assert(list && list->proc_ptrs);
  num_tids = list->num_tids;

  /* add newly created processes/threads */
  new_processes(list, screen, options);

  /* update statistics */
  for(proc = list->processes; proc; proc = proc->next) {
    FILE*     fstat;
    char      sub_task_name[100] = { 0 };
    double    elapsed;
    unsigned long   utime = 0, stime = 0;
    unsigned long   prev_cpu_time, curr_cpu_time;
    int             proc_id, zz;
    struct timeval  now;

    if (proc->dead) {
      num_dead++;
      continue;
    }

    /* Compute %CPU, retrieve processor ID. */
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
           "%*d (%*[^)]) %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",
           &utime, &stime);
      if (n != 2)
        utime = stime = 0;

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


/* Scan list of processes and deallocates the dead ones, compacting the list. */
void compact_proc_list(struct process_list* const list)
{
  struct process* p;
  int i;

  for(p = list->processes; p; p = p->next) {
    if (p && p->next && p->next->dead) {
      struct process* to_delete = p->next;
      hash_del(to_delete->tid);
      p->next = to_delete->next;
      done_proc(to_delete);
      free(to_delete);
      list->num_tids--;
    }
  }
  /* special case for 1st element */
  if (list->processes->dead) {
    struct process* to_delete = list->processes;
    hash_del(to_delete->tid);
    list->processes = to_delete->next;
    done_proc(to_delete);
    free(to_delete);
    list->num_tids--;
  }

  /* update pointers */
  i = 0;
  for(p = list->processes; p; p = p->next) {
    list->proc_ptrs[i] = p;
    i++;
  }
}


/*
 * When threads are not displayed, this function accumulates
 * per-thread statistics in the parent process (which is also a
 * thread).
 */
void accumulate_stats(const struct process_list* const list)
{
  int zz;
  struct process* p;

  p = list->processes;
  for(p = list->processes; p; p = p->next) {
    if (p->pid != p->tid) {
      struct process* owner;

      if (p->dead)
        continue;

      /* find the owner */
      owner = hash_get(p->pid);
      assert(owner);

      /* accumulate in owner process */
      owner->cpu_percent += p->cpu_percent;
      for(zz = 0; zz < p->num_events; zz++) {
        /* as soon as one thread has invalid value, skip entire process. */
        if (p->values[zz] == 0xffffffff) {
          owner->values[zz] = 0xffffffff;
          break;
        }
        owner->values[zz] += p->values[zz];
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
  struct process* p;

  for(p = list->processes; p; p = p->next) {
    if (p->dead)
      continue;
    /* only consider 'main' processes (not threads) */
    if (p->pid == p->tid) {
      int zz;
      p->cpu_percent = 0;
      for(zz = 0; zz < p->num_events; zz++) {
        p->values[zz] = 0;
      }
    }
  }
}
