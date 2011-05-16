/*
 * tiptop
 *
 * Author: Erven Rohou
 *
 * License: GNU General Public License version 2.
 *
 */

#include <assert.h>

#if defined(HAS_CURSES)
#include <curses.h>
#endif

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
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
#include "screen.h"
#include "utils.h"


extern int debug;

static float  delay = 2;
static int    help = 0;
static int    idle = 0;
static int    max_iter = 0;
static int    show_threads = 0;
static int    show_user = 0;
static char*  watch_name = NULL;
static pid_t  watch_pid = 0;
static int    watch_uid = -1;

/* CPU activity below which a thread is considered inactive */
static float  cpu_threshold = 0.00001;


static struct timeval tv;

static char* message = NULL;
static char  tmp_message[100];


static char* header = NULL;


static void usage(const char* name)
{
  fprintf(stderr, "Usage: %s flags\n", name);
  fprintf(stderr, "\t-b             run in batch mode\n");
  fprintf(stderr, "\t--cpu-min m    minimum %%CPU to display a process\n");
  fprintf(stderr, "\t-d delay       delay in seconds between refreshes\n");
  fprintf(stderr, "\t-g             debug\n");
  fprintf(stderr, "\t-h             print this message\n");
  fprintf(stderr, "\t-H             show threads\n");
  fprintf(stderr, "\t-i             also display idle processes\n");
  fprintf(stderr, "\t--list-screens display list of available screens\n");
  fprintf(stderr, "\t-n num         max number of refreshes\n");
  fprintf(stderr, "\t-S num         screen number to display\n");
  fprintf(stderr, "\t-u userid      only show user's processes\n");
  fprintf(stderr, "\t-U             show user name\n");
  fprintf(stderr, "\t-w pid|name    watch this process (highlighted)\n");
  return;
}


/* For each process/thread in the list, generate the text form, ready
 * to be printed.
 */
static void build_rows(struct process_list* proc_list, screen_t* s)
{
  int i, num_tids;
  struct process* p;
  char*  row;
  int    row_alloc;
  char   tmp[100];

  num_tids = proc_list->num_tids;
  p = proc_list->processes;

  /* For all processes/threads */
  for(i=0; i < num_tids; ++i) {
    char substr[100];
    int  col;

    /* display a '+' sign after processes made of multiple threads */
    int thr = ((!show_threads) && (p[i].num_threads > 1)) ? '+' : ' ';

    if (p[i].tid == 0)  /* dead */
      continue;

    if (show_user)
      sprintf(tmp, "%5d%c %-10s ", p[i].tid, thr, p[i].username);
    else
      sprintf(tmp, "%5d%c ", p[i].tid, thr);
    row = str_init(tmp, &row_alloc);

    for(col = 0; col < s->num_columns; col++) {
      char* fmt = s->columns[col].format;

      switch(s->columns[col].data.type) {
      case CPU_TOT: {
        sprintf(substr, fmt, p[i].cpu_percent);
        break;
      }

      case CPU_SYS: {
        sprintf(substr, fmt, p[i].cpu_percent_s);
        break;
      }

      case CPU_USER: {
        sprintf(substr, fmt, p[i].cpu_percent_u);
        break;
      }

      case COMPUT_RAW: {
        int counter = s->columns[col].data.param1;
        uint64_t delta;
        if (p[i].values[counter] == 0xffffffff) {
          sprintf(substr, "%s",  s->columns[col].error_field);
        }
        else {
          delta = p[i].values[counter] - p[i].prev_values[counter];
          sprintf(substr, fmt, delta);
        }
      }
        break;

      case COMPUT_RAW_M: {
        int counter = s->columns[col].data.param1;
        uint64_t delta;
        if (p[i].values[counter] == 0xffffffff) {
          sprintf(substr, "%s",  s->columns[col].error_field);
        }
        else {
          delta = p[i].values[counter] - p[i].prev_values[counter];
          sprintf(substr, fmt, delta / 1000000.0);
        }
      }
        break;

      case COMPUT_ABS: {
        int counter = s->columns[col].data.param1;
        if (p[i].values[counter] == 0xffffffff) {
          sprintf(substr, "%s",  s->columns[col].error_field);
        }
        else {
          sprintf(substr, fmt, p[i].values[counter]);
        }
      }
        break;

      case COMPUT_RATIO: {
        int counter1 = s->columns[col].data.param1;
        int counter2 = s->columns[col].data.param2;
        uint64_t delta1, delta2;

        if ((p[i].values[counter1] == 0xffffffff) ||
            (p[i].values[counter2] == 0xffffffff)) {
          sprintf(substr, "%s",  s->columns[col].error_field);
        }
        else {
          delta1 = p[i].values[counter1] - p[i].prev_values[counter1];
          delta2 = p[i].values[counter2] - p[i].prev_values[counter2];
          if (delta2 != 0)
            sprintf(substr, fmt, 1.0*delta1/delta2);
          else {
            sprintf(substr, "%s", s->columns[col].empty_field);
          }
        }
      }
        break;

      case COMPUT_PERCENT: {
        int counter1 = s->columns[col].data.param1;
        int counter2 = s->columns[col].data.param2;
        uint64_t delta1, delta2;
        if ((p[i].values[counter1] == 0xffffffff) ||
            (p[i].values[counter2] == 0xffffffff)) {
          sprintf(substr, "%s",  s->columns[col].error_field);
        }
        else {
          delta1 = p[i].values[counter1] - p[i].prev_values[counter1];
          delta2 = p[i].values[counter2] - p[i].prev_values[counter2];
          if (delta2 != 0)
            sprintf(substr, fmt, 100.0*delta1/delta2);
          else {
            sprintf(substr, "%s", s->columns[col].empty_field);
          }
        }
      }
        break;

      default:
        fprintf(stderr, "Internal error: unknown column type.\n");
        exit(EXIT_FAILURE);
      }

      row = str_append(row, &row_alloc, substr);
      row = str_append(row, &row_alloc, " ");
    }
    sprintf(substr, "%s\n", p[i].name);
    row = str_append(row, &row_alloc, substr);

    strcpy(p[i].txt, row);
    free(row);
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
static void batch_mode(struct process_list* proc_list, screen_t* screen)
{
  int   num_iter = 0;
  struct process* p;

  tv.tv_sec = 0;
  tv.tv_usec = 200000;  /* 200 ms for first iteration */

  /* Print various information about this run */
  printf("tiptop -");
  fflush(stdout);
  system("uptime");
  system("date");
  system("uname -a");
  printf("delay: %.2f  idle: %d  threads: %d\n", delay, idle, show_threads);
  if (watch_pid) {
    printf("watching pid %d\n", watch_pid);
  }
  else if (watch_name) {
    printf("watching pid '%s'\n", watch_name);
  }
  if (watch_uid != -1) {
    struct passwd* passwd = getpwuid(watch_uid);
    assert(passwd);
    printf("watching uid %d '%s'\n", watch_uid, passwd->pw_name);
  }

  printf("\n%s\n", header);

  for(num_iter=0; !max_iter || num_iter < max_iter; num_iter++) {
    int i;

    /* update the list of processes/threads and accumulate info if
       needed */
    update_proc_list(proc_list, screen, watch_uid);
    if (!show_threads)
      accumulate_stats(proc_list);

    p = proc_list->processes;

    /* generate the text version of all rows */
    build_rows(proc_list, screen);

    /* sort by %CPU */
    qsort(p, proc_list->num_tids, sizeof(struct process), cmp_cpu);

    for(i=0; i < proc_list->num_tids; i++) {
      if (p[i].pid == 0)  /* dead */
        continue;

      /* not active, skip */
      if (!idle && (p[i].cpu_percent < cpu_threshold))
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
static int handle_key()
{
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
    if (delay < 0.1)
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

  else if (c == 'U')
    show_user = 1 - show_user;

  else if (c == 'u') {
    char  str[100];  /* buffer overflow? */
    move(2,0);
    printw("Which user (blank for all): ");
    echo();
    nocbreak();
    getstr(str);
    cbreak();
    noecho();
    if (str[0] == '\0') {  /* blank */
      watch_uid = -1;
    }
    else if (isdigit(str[0])) {
      watch_uid = atoi(str);
    }
    else {
      struct passwd*  passwd;
      passwd = getpwnam(str);
      if (passwd) {
        watch_uid = passwd->pw_uid;
      }
      else {
        message = "User name does not exist.";
      }
    }
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

  else if (c == 'h') {
    help = 1 - help;
  }

  return c;
}


/* Main execution loop in live mode. Builds the list of processes,
 * collects statistics, and prints using curses. Repeats after some
 * delay, also catching key presses.
 */
static int live_mode(struct process_list* proc_list, screen_t* screen)
{
  WINDOW*         help_win;
  fd_set          fds;
  struct process* p;
  int             num_iter = 0;
  int             with_colors = 0;

  /* start curses */
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);

  help_win = newwin(10, 40, 10, 10);

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
    update_proc_list(proc_list, screen, watch_uid);
    if (!show_threads)
      accumulate_stats(proc_list);

    p = proc_list->processes;

    /* prepare for select */
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    /* generate the text version of all rows */
    build_rows(proc_list, screen);

    /* sort by %CPU */
    qsort(p, proc_list->num_tids, sizeof(struct process), cmp_cpu);

    printed = 0;
    /* Iterate over all threads */
    for(i=0; i < proc_list->num_tids; i++) {
      if (p[i].pid == 0)  /* dead */
        continue;

      /* not active, skip */
      if (!idle && (p[i].cpu_percent < cpu_threshold))
        continue;

      /* highlight watched process, if any */
      if (with_colors) {
        if ((p[i].tid == watch_pid) ||
            (watch_name && strstr(p[i].name, watch_name)))
          attron(COLOR_PAIR(3));
      }

      if (show_threads || (p[i].pid == p[i].tid)) {
        printw(p[i].txt);
        printed++;
      }

      if (with_colors)
        attroff(COLOR_PAIR(3));

      if (printed >= LINES - 5)  /* stop printing at bottom of window */
        break;
    }

    move(1, 0);
    printw("Tasks: %3d total, %3d running", proc_list->num_tids, printed);
    if (watch_uid != -1) {
      move(1, 35);
      printw("Watching uid: %5d\n", watch_uid);
    }

    /* print the screen name, make sure it fits, or truncate */
    if (with_colors)
      attron(COLOR_PAIR(4));
    if (35 + 20 + 11 + strlen(screen->name) < COLS) {
      move(1, COLS - 11 - strlen(screen->name));
      printw("screen %2d: %s\n", screen->id, screen->name);
    }
    else {
      char screen_str[50];
      move(1, 35 + 20);
      printw("screen %2d: ", screen->id);
      sprintf(screen_str, "%s\n", screen->name);
      screen_str[COLS - 35 - 20 - 11] = '\0';
      printw("%s", screen_str);
    }
    if (with_colors)
      attroff(COLOR_PAIR(4));

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
    if (help) {
      box(help_win, 0, 0);
      wmove(help_win, 0, 10);
      wprintw(help_win, " Help ");
      wmove(help_win, 1, 1);
      wprintw(help_win, "h to close");
      wrefresh(help_win);
    }

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
      if (c == 'U') {
        free(header);
        header = gen_header(screen, show_user);
      }
      if ((c == '+') || (c == '-') || (c == KEY_LEFT) || (c == KEY_RIGHT)) {
        return c;
      }
      if (c == 'u')  /* change user, need to rebuild list of processes */
        return c;
    }

    tv.tv_sec = delay;
    tv.tv_usec = (delay - tv.tv_sec) * 1000000.0;
  }
  endwin();  /* stop curses */
  return 'q';
}
#endif


int main(int argc, char* argv[])
{
  int i, key = 0;
  int list_scr = 0;
  struct process_list* proc_list;
  screen_t* screen = NULL;
  int screen_num = 0;
#if defined(HAS_CURSES)
  int  batch = 0;
#else
  int  batch = 1;
#endif

  /* If we are set-uid, take advantage of it. If it fails, never mind. */
  setuid(0);

  /* Check OS to make sure we can run. */
  check();

  /* Parse command line arguments. */
  for(i=1; i < argc; i++) {
    if (strcmp(argv[i], "-b") == 0) {
      batch = 1;
    }

    if (strcmp(argv[i], "--cpu-min") == 0) {
      if (i+1 < argc) {
        cpu_threshold = (float)atof(argv[i+1]);
        i++;
      }
      else {
        fprintf(stderr, "Missing value after --cpu-min.\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strcmp(argv[i], "-d") == 0) {
      if (i+1 < argc) {
        delay = (float)atof(argv[i+1]);
        if (delay < 0.1)
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

    if (strcmp(argv[i], "--list-screens") == 0) {
      list_scr = 1;
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

    if (strcmp(argv[i], "-S") == 0) {
      if (i+1 < argc) {
        screen_num = atoi(argv[i+1]);
        i++;
      }
      else {
        fprintf(stderr, "Missing screen number after -S.\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strcmp(argv[i], "-U") == 0) {
      show_user = 1;
    }

    if (strcmp(argv[i], "-u") == 0) {
      if (i+1 < argc) {
        if (isdigit(argv[i+1][0])) {
          watch_uid = atoi(argv[i+1]);
        }
        else {
          struct passwd* passwd = getpwnam(argv[i+1]);
          if (!passwd) {
            fprintf(stderr, "User name '%s' does not exist.\n", argv[i+1]);
            exit(EXIT_FAILURE);
          }
          watch_uid = passwd->pw_uid;
        }
        i++;
      }
      else {
        fprintf(stderr, "Missing user name after -u.\n");
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
  init_screen();

  if (list_scr) {
    list_screens();
    exit(0);
  }

  do {
    screen = get_screen(screen_num);
    if (!screen) {
      fprintf(stderr, "No such screen.\n");
      exit(EXIT_FAILURE);
    }

    header = gen_header(screen, show_user);

    /* initialize the list of processes, and then run */
    proc_list = init_proc_list();
    if (batch) {
      batch_mode(proc_list, screen);
      key = 'q';
    }
#if defined(HAS_CURSES)
    else {
      key = live_mode(proc_list, screen);
      if ((key == '+')  || (key == KEY_RIGHT)) {
        screen_num = (screen_num + 1) % get_num_screens();
        done_proc_list(proc_list);
      }
      if ((key == '-') || (key == KEY_LEFT)) {
        int n = get_num_screens();
        screen_num = (screen_num + n - 1) % n;
        done_proc_list(proc_list);
      }
      if (key == 'u') {
        done_proc_list(proc_list);
      }
    }
#endif
  } while (key != 'q');

  /* done, free memory (makes valgrind happy) */
  free(header);
  delete_screens();
  done_proc_list(proc_list);
  if (watch_name)
    free(watch_name);

  return 0;
}
