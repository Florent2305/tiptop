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
#include <time.h>
#include <unistd.h>

#include "conf.h"
#include "helpwin.h"
#include "options.h"
#include "pmc.h"
#include "process.h"
#include "requisite.h"
#include "screen.h"


struct option options;

static struct timeval tv;

static char* message = NULL;


static char* header = NULL;


/* For each process/thread in the list, generate the text form, ready
 * to be printed.
 */
static void build_rows(struct process_list* proc_list, screen_t* s, int width)
{
  int i, num_tids;
  struct process* p;

  assert(TXT_LEN > 20);

  num_tids = proc_list->num_tids;
  p = proc_list->processes;

  /* For all processes/threads */
  for(i=0; i < num_tids; ++i) {
    int   col, written;
    char* row = p[i].txt;       /* the row we are building */
    int   remaining = TXT_LEN;  /* remaining bytes in row */
    int   thr = ' ';

    p[i].skip = 1;  /* first, assume not ready */

    if ((p[i].dead == 1) && (!options.sticky)) /* dead */
      continue;

    /* not active, skip */
    if (!options.idle && (p[i].cpu_percent < options.cpu_threshold))
      continue;

    /* only some tasks are monitored, skip those that do not qualify */
    if (((options.only_pid) && (p[i].tid != options.only_pid)) ||
        (options.only_name && options.show_cmdline &&
                                  !strstr(p[i].cmdline, options.only_name)) ||
        (options.only_name && !options.show_cmdline &&
                                  !strstr(p[i].name, options.only_name)))
      continue;

    if ((width != -1) && (width < remaining))
      remaining = width;

    /* display a '+' sign after processes made of multiple threads */
    if (p[i].num_threads > 1) {
      if (p[i].tid == p[i].pid)
        thr = '+';
      else
        thr = '-';
    }

    if (options.show_user)
      written = snprintf(row, remaining, "%5d%c %-10s ", p[i].tid, thr,
                                                         p[i].username);
    else
      written = snprintf(row, remaining, "%5d%c ", p[i].tid, thr);
    row += written;
    remaining -= written;

    for(col = 0; col < s->num_columns; col++) {
      const char* const fmt = s->columns[col].format;

      switch(s->columns[col].data.type) {
      case CPU_TOT:
        written = snprintf(row, remaining, fmt, p[i].cpu_percent);
        break;

      case CPU_SYS:
        written = snprintf(row, remaining, fmt, p[i].cpu_percent_s);
        break;

      case CPU_USER:
        written = snprintf(row, remaining, fmt, p[i].cpu_percent_u);
        break;

      case PROC_ID:
        if (p[i].proc_id != -1)
          written = snprintf(row, remaining, fmt, p[i].proc_id);
        else
          written = snprintf(row, remaining, "%s", s->columns[col].error_field);
        break;

      case COMPUT_RAW: {
        int counter = s->columns[col].data.param1;
        if (p[i].values[counter] == 0xffffffff)
          written = snprintf(row, remaining, "%s", s->columns[col].error_field);
        else {
          uint64_t delta = p[i].values[counter] - p[i].prev_values[counter];
          written = snprintf(row, remaining, fmt, delta);
        }
      }
        break;

      case COMPUT_RAW_M: {
        int counter = s->columns[col].data.param1;
        if (p[i].values[counter] == 0xffffffff)
          written = snprintf(row, remaining, "%s", s->columns[col].error_field);
        else {
          uint64_t delta = p[i].values[counter] - p[i].prev_values[counter];
          written = snprintf(row, remaining, fmt, delta / 1000000.0);
        }
      }
        break;

      case COMPUT_ABS: {
        int counter = s->columns[col].data.param1;
        if (p[i].values[counter] == 0xffffffff) {
          written = snprintf(row, remaining, "%s", s->columns[col].error_field);
        }
        else
          written = snprintf(row, remaining, fmt, p[i].values[counter]);
      }
        break;

      case COMPUT_ABS_M: {
        int counter = s->columns[col].data.param1;
        if (p[i].values[counter] == 0xffffffff)
          written = snprintf(row, remaining, "%s", s->columns[col].error_field);
        else
          written = snprintf(row, remaining,fmt,p[i].values[counter]/1000000.0);
      }
        break;

      case COMPUT_RATIO: {
        int counter1 = s->columns[col].data.param1;
        int counter2 = s->columns[col].data.param2;

        if ((p[i].values[counter1] == 0xffffffff) ||
            (p[i].values[counter2] == 0xffffffff)) {
          written = snprintf(row, remaining, "%s", s->columns[col].error_field);
        }
        else {
          uint64_t delta1 = p[i].values[counter1] - p[i].prev_values[counter1];
          uint64_t delta2 = p[i].values[counter2] - p[i].prev_values[counter2];
          if (delta2 != 0)
            written = snprintf(row, remaining, fmt, 1.0*delta1/delta2);
          else
            written = snprintf(row, remaining,"%s",s->columns[col].empty_field);
        }
      }
        break;

      case COMPUT_PERCENT: {
        int counter1 = s->columns[col].data.param1;
        int counter2 = s->columns[col].data.param2;
        if ((p[i].values[counter1] == 0xffffffff) ||
            (p[i].values[counter2] == 0xffffffff)) {
          written = snprintf(row, remaining, "%s", s->columns[col].error_field);
        }
        else {
          uint64_t delta1 = p[i].values[counter1] - p[i].prev_values[counter1];
          uint64_t delta2 = p[i].values[counter2] - p[i].prev_values[counter2];
          if (delta2 != 0)
            written = snprintf(row, remaining, fmt, 100.0*delta1/delta2);
          else
            written = snprintf(row, remaining,"%s",s->columns[col].empty_field);
        }
      }
        break;

      default:
        fprintf(stderr, "Internal error: unknown column type.\n");
        exit(EXIT_FAILURE);
      }

      /* man snprintf: The functions snprintf() and vsnprintf() do not
       write more than size bytes (including the trailing '\0').  If
       the output was truncated due to this limit then the return
       value is the number of characters (not including the trailing
       '\0') which would have been written to the final string if
       enough space had been available.  Thus, a return value of size
       or more means that the output was truncated.  */
      if (written >= remaining) {
        remaining = 0;
        break;  /* line is full */
      }

      row += written;
      remaining -= written;

      /* add space after column, if it fits */
      if (remaining >= 2) {
        row[0] = ' ';
        row[1] = '\0';
        row++;
        remaining--;
      }
    }

    if (options.show_cmdline)
      strncpy(row, p[i].cmdline, remaining);
    else
      strncpy(row, p[i].name, remaining);

    if (remaining)
      row[remaining-1] = '\0';

    p[i].skip = 0;
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
  int   num_printed, foo;
  struct process* p;

  tv.tv_sec = 0;
  tv.tv_usec = 200000;  /* 200 ms for first iteration */

  /* Print various information about this run */
  printf("tiptop -");
  fflush(stdout);
  foo = system("uptime");
  foo = system("date");
  foo = system("uname -a");
  printf("delay: %.2f  idle: %d  threads: %d\n",
         options.delay, (int)options.idle, (int)options.show_threads);
  if (options.watch_pid) {
    printf("watching pid %d\n", options.watch_pid);
  }
  else if (options.watch_name) {
    printf("watching pid '%s'\n", options.watch_name);
  }
  if (options.only_pid) {
    printf("only pid %d\n", options.only_pid);
  }
  else if (options.only_name) {
    printf("only pid '%s'\n", options.only_name);
  }
  if (options.watch_uid != -1) {
    struct passwd* passwd = getpwuid(options.watch_uid);
    assert(passwd);
    printf("watching uid %d '%s'\n", options.watch_uid, passwd->pw_name);
  }

  header = gen_header(screen, options.show_user,
                      options.show_timestamp && options.batch,
                      options.show_epoch && options.batch,
                      TXT_LEN - 1);

  printf("Screen %d: %s\n", screen->id, screen->name);
  printf("\n%s\n", header);

  for(num_iter=0; !options.max_iter || num_iter<options.max_iter; num_iter++) {
    unsigned int epoch = 0;
    int i;

    /* update the list of processes/threads and accumulate info if
       needed */
    if (options.show_epoch)
      epoch = time(NULL);

    if (update_proc_list(proc_list, screen, &options) &&
        (!options.sticky)) {
      compact_proc_list(proc_list);
    }
    if (!options.show_threads)
      accumulate_stats(proc_list);

    p = proc_list->processes;

    /* generate the text version of all rows */
    build_rows(proc_list, screen, -1);

    /* sort by %CPU */
    qsort(p, proc_list->num_tids, sizeof(struct process), cmp_cpu);

    num_printed = 0;
    for(i=0; i < proc_list->num_tids; i++) {

      if (p[i].skip)
        continue;

      if (options.show_threads || (p[i].pid == p[i].tid)) {
        if (options.show_timestamp)
          printf("%6d ", num_iter);
        if (options.show_epoch)
          printf("%10u ", epoch);
        printf("%s%s", p[i].txt, p[i].dead ? " DEAD" : "");

        /* if the process is being watched */
        if ((p[i].tid == options.watch_pid) ||
            (options.watch_name && options.show_cmdline &&
                                  strstr(p[i].cmdline, options.watch_name)) ||
            (options.watch_name && !options.show_cmdline &&
                                  strstr(p[i].name, options.watch_name)))
          printf(" <---");
        printf("\n");
        num_printed++;
      }
    }

    if (num_printed)
      printf("\n");
    fflush(stdout);
    /* wait some delay */
    select(0, NULL, NULL, NULL, &tv);

    /* prepare for next select */
    tv.tv_sec = options.delay;
    tv.tv_usec = (options.delay - tv.tv_sec) * 1000000.0;
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
    options.debug = 1 - options.debug;

  else if (c == 'c')
    options.show_cmdline = 1 - options.show_cmdline;

  else if ((c == 'd') || (c == 's')) {
    move(2,0);
    printw("Change delay from %.2f to: ", options.delay);
    echo();
    nocbreak();
    scanw("%f", &options.delay);
    if (options.delay < 0.1)
      options.delay = 1.0;
    tv.tv_sec = options.delay;
    tv.tv_usec = (options.delay - tv.tv_sec)*1000000.0;
    cbreak();
    noecho();
  }

  else if (c == 'H') {
    options.show_threads = 1 - options.show_threads;
  }

  else if (c == 'i')
    options.idle = 1 - options.idle;

  else if (c == 'K') {
    if (options.show_kernel) {
      options.show_kernel = 0;
      message = "Kernel mode Off";
    }
    else {
      if (geteuid() == 0) {
        options.show_kernel = 1;
        message = "Kernel mode On";
      }
      else {
        message = "Kernel mode only available to root.";
        c = ' ';  /* do not return the 'K' to the upper level, since
                     it was ignored. */
      }
    }
  }


  else if (c == 'k') {  /* initialize string to 0s */
    char str[10] = { 0 };
    int  kill_pid, kill_sig, kill_res;
    move(2,0);
    printw("PID to kill: ");
    echo();
    nocbreak();
    getnstr(str, sizeof(str) - 1);
    if (!isdigit(str[0])) {
      move(2,0);
      message = "Not valid";
    }
    else {
      kill_pid = atoi(str);
      move(2,0);
      printw("Kill PID %d with signal [15]: ", kill_pid);
      getnstr(str, sizeof(str) - 1);
      kill_sig = atoi(str);
      if (kill_sig == 0)
        kill_sig = 15;
      kill_res = kill(kill_pid, kill_sig);
      if (kill_res == -1) {
        char tmp_message[100];
        move(2,0);
        snprintf(tmp_message, sizeof(tmp_message),
                 "Kill of PID '%d' with '%d' failed: %s",
                 kill_pid, kill_sig, strerror(errno));
        message = tmp_message;
      }
    }
    cbreak();
    noecho();
  }

  else if (c == 'p') {
    char str[100] = { 0 };  /* initialize string to 0s */
    move(2,0);
    printw("Only display process: ");
    echo();
    nocbreak();
    getnstr(str, sizeof(str)-1);  /* keep final '\0' as string delimiter */
    options.only_pid = atoi(str);
    if (options.only_name) {
      free(options.only_name);
      options.only_name = NULL;
    }
    if (!options.only_pid && strcmp(str, "")) {
      options.only_name = strdup(str);
    }
    cbreak();
    noecho();
  }

  else if (c == 'U')
    options.show_user = 1 - options.show_user;

  else if (c == 'u') {
    char str[100] = { 0 };  /* initialize string to 0s */
    move(2,0);
    printw("Which user (blank for all): ");
    echo();
    nocbreak();
    getnstr(str, sizeof(str) - 1);  /* keep final '\0' as string delimiter */
    cbreak();
    noecho();
    if (str[0] == '\0') {  /* blank */
      options.watch_uid = -1;
    }
    else if (isdigit(str[0])) {
      options.watch_uid = atoi(str);
    }
    else {
      struct passwd*  passwd;
      passwd = getpwnam(str);
      if (passwd) {
        options.watch_uid = passwd->pw_uid;
      }
      else {
        message = "User name does not exist.";
      }
    }
  }

  else if (c == 'w') {
    char str[100] = { 0 };  /* initialize string to 0s */
    move(2,0);
    printw("Watch process: ");
    echo();
    nocbreak();
    getnstr(str, sizeof(str)-1);  /* keep final '\0' as string delimiter */
    options.watch_pid = atoi(str);
    if (options.watch_name) {
      free(options.watch_name);
      options.watch_name = NULL;
    }
    if (!options.watch_pid && strcmp(str, "")) {
      options.watch_name = strdup(str);
    }
    cbreak();
    noecho();
  }

  else if (c == 'h') {
    options.help = 1 - options.help;
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

  /* Prepare help window */
  help_win = prepare_help_win(screen);

  if (has_colors()) {
    /* initialize curses colors */
    with_colors = 1;
    start_color();
    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    init_pair(2, COLOR_WHITE, COLOR_BLACK);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    init_pair(4, COLOR_YELLOW, COLOR_BLACK);
    init_pair(5, COLOR_RED, COLOR_BLACK);
    attron(COLOR_PAIR(0));
  }

  tv.tv_sec = 0;
  tv.tv_usec = 200000; /* 200 ms for first iteration */

  header = gen_header(screen, options.show_user,
                      options.show_timestamp && options.batch,
                      options.show_epoch && options.batch,
                      COLS - 1);

  for(num_iter=0; !options.max_iter || num_iter<options.max_iter; num_iter++) {
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

    if (options.debug) {
      move(0, COLS-8);
      printw(" [debug]");
    }

    if (options.show_epoch) {
      unsigned int epoch = time(NULL);
      move(LINES-1, COLS-18);
      printw("Epoch: %u", epoch);
    }

    if (options.show_timestamp) {
      move(LINES-1, 0);
      printw("Iteration: %u", num_iter);
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

    /* update the list of processes/threads and accumulate info if needed */
    if (update_proc_list(proc_list, screen, &options) && !options.sticky) {
      compact_proc_list(proc_list);
    }
    if (!options.show_threads)
      accumulate_stats(proc_list);

    p = proc_list->processes;

    /* prepare for select */
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    /* generate the text version of all rows */
    build_rows(proc_list, screen, COLS - 1);

    /* sort by %CPU */
    qsort(p, proc_list->num_tids, sizeof(struct process), cmp_cpu);

    printed = 0;
    /* Iterate over all threads */
    for(i=0; i < proc_list->num_tids; i++) {

      if (p[i].skip)
        continue;

      /* highlight watched process, if any */
      if (with_colors) {
        if (p[i].dead == 1) {
          attron(COLOR_PAIR(5));
        }
        else if ((p[i].tid == options.watch_pid) ||
                 (options.watch_name && options.show_cmdline &&
                                strstr(p[i].cmdline, options.watch_name)) ||
                 (options.watch_name && !options.show_cmdline &&
                                strstr(p[i].name, options.watch_name)))
          attron(COLOR_PAIR(3));
      }

      if (options.show_threads || (p[i].pid == p[i].tid)) {
        printw("%s\n", p[i].txt);
        printed++;
      }

      if (with_colors)
        attroff(COLOR_PAIR(3));

      if (printed >= LINES - 5)  /* stop printing at bottom of window */
        break;
    }

    move(1, 0);
    printw("Tasks: %3d total, %3d running", proc_list->num_tids, printed);
    if (options.watch_uid != -1) {
      move(1, 35);
      printw("Watching uid: %5d\n", options.watch_uid);
    }

    /* print the screen name, make sure it fits, or truncate */
    if (with_colors)
      attron(COLOR_PAIR(4));
    if (35 + 20 + 11 + strlen(screen->name) < COLS) {
      move(1, COLS - 11 - strlen(screen->name));
      printw("screen %2d: %s\n", screen->id, screen->name);
    }
    else {
      char screen_str[50] = { 0 };
      move(1, 35 + 20);
      snprintf(screen_str, sizeof(screen_str) - 1, "%s\n", screen->name);
      screen_str[COLS - 35 - 20 - 11] = '\0';  /* truncate */
      printw("screen %2d: %s", screen->id, screen_str);
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
    if (options.help) {
      show_help_win(help_win, screen);
    }

    /* wait some delay, or until a key is pressed */
    num_fd = select(1 + STDIN_FILENO, &fds, NULL, NULL, &tv);
    if (num_fd > 0) {
      int c = handle_key();
      if (c == 'q')
        break;
      if (c == 'H') {
        if (options.show_threads) {
          reset_values(proc_list);
          message = "Show threads On";
        }
        else
          message = "Show threads Off";
      }
      if (c == 'U') {
        free(header);
        header = gen_header(screen,options.show_user, options.show_timestamp,
                            options.show_epoch, COLS - 1);
      }
      if ((c == '+') || (c == '-') || (c == KEY_LEFT) || (c == KEY_RIGHT)) {
        return c;
      }
      if ((c == 'u') || (c == 'K')) /* need to rebuild list of tasks */
        return c;
    }

    tv.tv_sec = options.delay;
    tv.tv_usec = (options.delay - tv.tv_sec) * 1000000.0;
  }
  endwin();  /* stop curses */
  return 'q';
}
#endif


int main(int argc, char* argv[])
{
  int key = 0;
  int list_scr = 0;
  struct process_list* proc_list;
  screen_t* screen = NULL;
  int screen_num = 0;

  /* Check OS to make sure we can run. */
  check();

  init_options(&options);
  read_config(&options);

  /* Parse command line arguments. */
  parse_command_line(argc, argv, &options, &list_scr, &screen_num);

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

    /* initialize the list of processes, and then run */
    proc_list = init_proc_list();
    if (options.batch) {
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
      if ((key == 'u') || (key == 'K')) {
        done_proc_list(proc_list);
      }
    }
#endif
  } while (key != 'q');

  /* done, free memory (makes valgrind happy) */
  free(header);
  delete_screens();
  done_proc_list(proc_list);
  if (options.watch_name)
    free(options.watch_name);

  return 0;
}
