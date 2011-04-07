/*
 * tiptop
 *
 * Author: Erven Rohou
 * 
 * License: GNU General Public License version 2.
 *
 */

#if defined(HAS_CURSES)
#include <curses.h>
#endif

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>

#include "conf.h"
#include "requisite.h"
#include "pmc.h"
#include "process.h"


int    debug = 0;

static float  delay = 2;
static int    idle = 0;
static int    max_iter = 0;
static int    show_threads = 0;
static char*  watch_name = NULL;
static pid_t  watch_pid = 0;

static struct timeval tv;

static char* message = NULL;
static char  tmp_message[100];


const char* const header =
  "  PID  %CPU   Minstr   Mcycles   IPC   ipc %MISS  %BMISS  COMMAND";



void usage(const char* name)
{
  fprintf(stderr, "Usage: %s flags\n", name);
  fprintf(stderr, "\t-b           run in batch mode\n");
  fprintf(stderr, "\t-d delay     delay in seconds between refreshes\n");
  fprintf(stderr, "\t-g           debug\n");
  fprintf(stderr, "\t-h           print this message\n");
  fprintf(stderr, "\t-H           show threads\n");
  fprintf(stderr, "\t-i           also display idle processes\n");
  fprintf(stderr, "\t-n num       max number of refreshes\n");
  fprintf(stderr, "\t-w pid|name  watch this process (highlighted)\n");
  return;
}


/* For each process/thread in the list, generate the text form, ready
 * to be printed.
 */
void build_rows(struct process_list* proc_list) { int
i, num_tids; struct process* p;

  num_tids = proc_list->num_tids;
  p = proc_list->processes;

  /* For all processes/threads */
  for(i=0; i < proc_list->num_tids; ++i) {
    uint64_t  insns, cycles, misses, brmisses;
    uint64_t  prev_insns, prev_cycles, prev_misses, prev_brmisses;

    /* display a '+' sign after processes made of multiple threads */
    char      thr = ((!show_threads) && (p[i].num_threads > 1)) ? '+' : ' ';


    if (p[i].tid == 0)  /* dead */
      continue;

    cycles = p[i].values[0];
    insns = p[i].values[1];
    misses = p[i].values[2];
    brmisses = p[i].values[3];

    prev_cycles = p[i].prev_values[0];
    prev_insns = p[i].prev_values[1];
    prev_misses = p[i].prev_values[2];
    prev_brmisses = p[i].prev_values[3];

    if (insns == prev_insns) {  /* no insn executed recently */
      if (cycles) {
        sprintf(p[i].txt,
                "%5d%c%5.1f           %8.2f        %4.2f                %s\n",
                p[i].tid,
                thr,
                p[i].cpu_percent,
                (cycles-prev_cycles)/1e6,
                1.0*insns/cycles,
                p[i].name);
      }
      else {
        sprintf(p[i].txt,
                "%5d%c%5.1f                                               %s\n",
                p[i].tid,
                thr,
                p[i].cpu_percent,
                p[i].name);
      }
    }
    else
      sprintf(p[i].txt,
              "%5d%c%5.1f %8.2f  %8.2f  %4.2f  %4.2f %5.2f  %6.2f  %s\n",
              p[i].tid,
              thr,
              p[i].cpu_percent,
              (insns-prev_insns)/1e6,
              (cycles-prev_cycles)/1e6,
              1.0*(insns-prev_insns)/(cycles-prev_cycles),
              1.0*insns/cycles,
              100.0*(misses-prev_misses)/(insns-prev_insns),
              100.0*(brmisses-prev_brmisses)/(insns-prev_insns),
              p[i].name);
  }
}


/* Utility function used by qsort to sort processes by decreasing %CPU. */
static int cmp_cpu(const void* p1, const void* p2)
{
  struct process* proc1 = (struct process*)p1;
  struct process* proc2 = (struct process*)p2;
  if (proc1->cpu_percent > proc2->cpu_percent)
    return -1;
  else if (proc1->cpu_percent == proc2->cpu_percent)
    return 0;
  else
    return 1;
}


/* Main execution loop in batch mode. Builds the list of processes,
 * collects statistics, and prints. Repeats after some delay.
 */
static void batch_mode(struct process_list* proc_list)
{
  int  num_iter = 0;
  struct process* p;

  tv.tv_sec = 0;
  tv.tv_usec = 200000;  /* 200 ms for first iteration */

  for(num_iter=0; !max_iter || num_iter < max_iter; num_iter++) {
    int i;

    printf("%s\n", header);

    /* update the list of processes/threads and accumulate info if
       needed */
    update_proc_list(proc_list);
    if (!show_threads)
      accumulate_stats(proc_list);

    p = proc_list->processes;

    /* generate the text version of all rows */
    build_rows(proc_list);

    /* sort by %CPU */
    qsort(p, proc_list->num_tids, sizeof(struct process), cmp_cpu);

    for(i=0; i < proc_list->num_tids; i++) {
      if (p[i].pid == 0)  /* dead */
        continue;

      /* no insn executed, skip */
      if (!idle && (p[i].values[1] == p[i].prev_values[1]))
        continue;

      /* In batch mode, if a process is being watched, only print this
         one. */
      if ((watch_pid && (p[i].tid != watch_pid)) ||
          (watch_name && !strstr(p[i].name, watch_name)))
        continue;

      if (show_threads || (p[i].pid == p[i].tid))
        printf("%s", p[i].txt);
    }

    printf("\n");
    /* wait some delay */
    select(0, NULL, NULL, NULL, &tv);

    /* prepare for next select */
    tv.tv_sec = delay;
    tv.tv_usec = (delay - tv.tv_sec) * 1000000.0;
  }
}


#if defined(HAS_CURSES)
/* Handle a key press.  Assumes that a key has been pressed and is
 * ready to read (will block otherwise).
 * Return 1 if 'q' (quit).
 */
static int handle_key() {
  int c;

  c = getch();
  if (c == 'q')
    ; /* nothing */
  else if (c == 'g')
    debug = 1 - debug;

  else if ((c == 'd') || (c == 's')) {
    move(2,0);
    printw("Change delay from %.2f to: ", delay);
    echo();
    nocbreak();
    scanw("%f", &delay);
    if (delay == 0)
      delay = 1.0;
    tv.tv_sec = delay;
    tv.tv_usec = (delay - tv.tv_sec)*1000000.0;
    cbreak();
    noecho();
  }

  else if (c == 'H') {
    show_threads = 1 - show_threads;
  }

  else if (c == 'i')
    idle = 1 - idle;

  else if (c == 'k') {
    char str[100];  /* buffer overflow? */
    int  kill_pid, kill_sig, kill_res;
    move(2,0);
    printw("PID to kill: ");
    echo();
    nocbreak();
    getstr(str);
    if (!isdigit(str[0])) {
      move(2,0);
      message = "Not valid";
    }
    else {
      kill_pid = atoi(str);
      move(2,0);
      printw("Kill PID %d with signal [15]: ", kill_pid);
      getstr(str);
      kill_sig = atoi(str);
      if (kill_sig == 0)
        kill_sig = 15;
      kill_res = kill(kill_pid, kill_sig);
      if (kill_res == -1) {
        move(2,0);
        sprintf(tmp_message, "Kill of PID '%d' with '%d' failed: %s",
                kill_pid, kill_sig, strerror(errno));
        message = tmp_message;
      }
    }
    cbreak();
    noecho();
  }

  else if (c == 'w') {
    char str[100];  /* buffer overflow? */
    move(2,0);
    printw("Watch process: ");
    echo();
    nocbreak();
    getstr(str);
    watch_pid = atoi(str);
    if (watch_name) {
      free(watch_name);
      watch_name = NULL;
    }
    if (!watch_pid && strcmp(str, "")) {
      watch_name = strdup(str);
    }
    cbreak();
    noecho();
  }
  return c;
}


/* Main execution loop in live mode. Builds the list of processes,
 * collects statistics, and prints using curses. Repeats after some
 * delay, also catching key presses.
 */
static void live_mode(struct process_list* proc_list)
{
  fd_set          fds;
  struct process* p;
  int             num_iter = 0;
  int             with_colors = 0;

  /* start curses */
  initscr();
  cbreak();
  noecho();

  if (has_colors()) {
    /* initialize curses colors */
    with_colors = 1;
    start_color();
    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    init_pair(2, COLOR_WHITE, COLOR_BLACK);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    init_pair(4, COLOR_YELLOW, COLOR_BLACK);
    attron(COLOR_PAIR(0));
  }

  tv.tv_sec = 0;
  tv.tv_usec = 200000; /* 200 ms for first iteration */

  for(num_iter=0; !max_iter || num_iter < max_iter; num_iter++) {
    double     total_cpu, total_ipc;
    int        i, zz, printed;
    int        num_fd;
    FILE*      f_uptime;
    char       buffer[100];

    /* print various info */
    erase();
    move(0, 0);
    printw("tiptop -");
    f_uptime = popen("uptime", "r");
    if (fgets(buffer, sizeof(buffer), f_uptime))
      printw(buffer);
    fclose(f_uptime);

    if (debug) {
      move(0, COLS-8);
      printw(" [debug]");
    }

    /* print main header */
    move(3,0);
    if (with_colors)
      attron(COLOR_PAIR(1));
    printw("%s", header);
    for(zz=strlen(header); zz < COLS-1; zz++)
      printw(" ");
    printw("\n");
    if (with_colors)
      attroff(COLOR_PAIR(1));

    /* update the list of processes/threads and accumulate info if
       needed */
    update_proc_list(proc_list);
    if (!show_threads)
      accumulate_stats(proc_list);

    p = proc_list->processes;

    /* prepare for select */
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    /* generate the text version of all rows */
    build_rows(proc_list);

    /* sort by %CPU */
    qsort(p, proc_list->num_tids, sizeof(struct process), cmp_cpu);

    printed = 0;
    total_cpu = 0.0;
    total_ipc = 0.0;
    /* Iterate over all threads */
    for(i=0; i < proc_list->num_tids; i++) {
      if (p[i].pid == 0)  /* dead */
        continue;

      /* no insn executed, skip */
      if (!idle && (p[i].values[1] == p[i].prev_values[1]))
        continue;

      /* highlight watched process, if any */
      if ((p[i].tid == watch_pid) ||
          (watch_name && strstr(p[i].name, watch_name)))
        if (with_colors)
          attron(COLOR_PAIR(3));

      if (show_threads || (p[i].pid == p[i].tid)) {
        printw(p[i].txt);
        printed++;
      }

      if (with_colors)
        attroff(COLOR_PAIR(3));
      
      if ((p[i].pid == p[i].tid) && (p[i].values[0] != p[i].prev_values[0])) {
        total_cpu += p[i].cpu_percent;
        total_ipc += p[i].cpu_percent*(p[i].values[1]-p[i].prev_values[1])/(p[i].values[0]-p[i].prev_values[0]);
      }

      if (printed >= LINES - 5)  /* stop printing at bottom of window */
        break;
    }

    /* print the total IPC at the bottom */
    if (with_colors)
      attron(COLOR_PAIR(4));
    if (total_cpu != 0.0) {
      printw("total %5.1f                     %4.2f\n",
             total_cpu, total_ipc/total_cpu);
    }
    if (with_colors)
      attroff(COLOR_PAIR(4));

    move(1, 0);
    printw("Tasks: %d total, %d running\n", proc_list->num_tids, printed);
  
    /* print message if any */
    if (message) {
      move(2, 0);
      if (with_colors)
        attron(COLOR_PAIR(1));
      printw("%s", message);
      if (with_colors)
        attroff(COLOR_PAIR(1));
      message = NULL;  /* reset message */
    }

    refresh();  /* display everything */


    /* wait some delay, or until a key is pressed */
    num_fd = select(1 + STDIN_FILENO, &fds, NULL, NULL, &tv);
    if (num_fd > 0) {
      int c = handle_key();
      if (c == 'q')
        break;
      if (c == 'H') {
        if (show_threads) {
          reset_values(proc_list);
          message = "Show threads On";
        }
        else
          message = "Show threads Off";
      }
    }

    tv.tv_sec = delay;
    tv.tv_usec = (delay - tv.tv_sec) * 1000000.0;
  }
  endwin();  /* start curses */
}
#endif


int main(int argc, char* argv[])
{
  int i;
  struct process_list* proc_list;
#if defined(HAS_CURSES)
  int  batch = 0;
#else
  int  batch = 1;
#endif

  /* Check OS to make sure we can run. */
  check();

  /* Parse command line arguments. */
  for(i=1; i < argc; i++) {
    if (strcmp(argv[i], "-b") == 0) {
      batch = 1;
    }

    if (strcmp(argv[i], "-d") == 0) {
      if (i+1 < argc) {
        delay = atof(argv[i+1]);
        if (delay == 0)
          delay = 1;
        i++;
      }
      else {
        fprintf(stderr, "Missing delay after -d.\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strcmp(argv[i], "-g") == 0) {
      debug = 1;
    }

    if (strcmp(argv[i], "-h") == 0) {
      usage(argv[0]);
      exit(0);
    }

    if (strcmp(argv[i], "-H") == 0) {
      show_threads = 1;
    }

    if (strcmp(argv[i], "-i") == 0) {
      idle = 1;
    }

    if (strcmp(argv[i], "-n") == 0) {
      if (i+1 < argc) {
        max_iter = atoi(argv[i+1]);
        i++;
      }
      else {
        fprintf(stderr, "Missing number of iterations after -n.\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strcmp(argv[i], "-w") == 0) {
      if (i+1 < argc) {
        watch_pid = atoi(argv[i+1]);
        if (watch_pid == 0)
          watch_name = strdup(argv[i+1]);
        i++;
      }
      else {
        fprintf(stderr, "Missing pid/name after -w.\n");
        exit(EXIT_FAILURE);
      }
    }
  }

  read_config();

  /* initialize the list of processes, and then run */
  proc_list = init_proc_list();
  if (batch)
    batch_mode(proc_list);
#if defined(HAS_CURSES)
  else
    live_mode(proc_list);
#endif


  /* done, free memory (makes valgrind happy) */
  done_proc_list(proc_list);
  if (watch_name)
    free(watch_name);

  return 0;
}
