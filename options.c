#include <ctype.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "options.h"
#include "version.h"


static void usage(const char* name)
{
  fprintf(stderr, "Usage: %s [option]\n", name);
  fprintf(stderr, "\t-b             run in batch mode\n");
  fprintf(stderr, "\t-c             use command line instead of process name\n");
  fprintf(stderr, "\t--cpu-min m    minimum %%CPU to display a process\n");
  fprintf(stderr, "\t-d delay       delay in seconds between refreshes\n");
  fprintf(stderr, "\t--epoch        add epoch at beginning of each line\n");
  fprintf(stderr, "\t-g             debug\n");
  fprintf(stderr, "\t-h --help      print this message\n");
  fprintf(stderr, "\t-H             show threads\n");
  fprintf(stderr, "\t-K --kernel    show kernel activity (only for root)\n");
  fprintf(stderr, "\t-i             also display idle processes\n");
  fprintf(stderr, "\t--list-screens display list of available screens\n");
  fprintf(stderr, "\t-n num         max number of refreshes\n");
  fprintf(stderr, "\t-S num         screen number to display\n");
  fprintf(stderr, "\t--sticky       keep final status of dead processes\n");
  fprintf(stderr, "\t--timestamp    add timestamp at beginning of each line\n");
  fprintf(stderr, "\t-u userid      only show user's processes\n");
  fprintf(stderr, "\t-U             show user name\n");
  fprintf(stderr, "\t-v             print version and exit\n");
  fprintf(stderr, "\t--version      print legalese and exit\n");
  fprintf(stderr, "\t-w pid|name    watch this process (highlighted)\n");
  return;
}


void init_options(struct option* opt)
{
  /* default status for options */
  memset(opt, 0, sizeof(*opt));
  opt->delay = 2;
  opt->cpu_threshold = 0.00001;
  opt->watch_uid = -1;
#if !defined(HAS_CURSES)
  /* make batch mode default if curses is not available */
  opt->batch = 1;
#endif
}


void parse_command_line(int argc, char* argv[],
                        struct option* options,
                        int* list_scr,
                        int* screen_num)
{
  int i;

  for(i=1; i < argc; i++) {
    if (strcmp(argv[i], "-b") == 0) {
      options->batch = 1;
    }

    if (strcmp(argv[i], "-c") == 0) {
      options->show_cmdline = 1;
    }

    if (strcmp(argv[i], "--cpu-min") == 0) {
      if (i+1 < argc) {
        options->cpu_threshold = (float)atof(argv[i+1]);
        i++;
      }
      else {
        fprintf(stderr, "Missing value after --cpu-min.\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strcmp(argv[i], "-d") == 0) {
      if (i+1 < argc) {
        options->delay = (float)atof(argv[i+1]);
        if (options->delay < 0.1)
          options->delay = 1;
        i++;
      }
      else {
        fprintf(stderr, "Missing delay after -d.\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strcmp(argv[i], "--epoch") == 0) {
      options->show_epoch = 1;
    }

    if (strcmp(argv[i], "-g") == 0) {
      options->debug = 1;
    }

    if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0)) {
      usage(argv[0]);
      exit(0);
    }

    if (strcmp(argv[i], "-H") == 0) {
      options->show_threads = 1;
    }

    if (strcmp(argv[i], "-i") == 0) {
      options->idle = 1;
    }

    if ((strcmp(argv[i], "-K") == 0) || (strcmp(argv[i], "--kernel") == 0)) {
      if (geteuid() == 0)
        options->show_kernel = 1;
      else {
        fprintf(stderr, "Kernel mode (-K --kernel) not available.\n");
        fprintf(stderr, "You are not root, or the binary is not setuid.\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strcmp(argv[i], "--list-screens") == 0) {
      *list_scr = 1;
    }

    if (strcmp(argv[i], "-n") == 0) {
      if (i+1 < argc) {
        options->max_iter = atoi(argv[i+1]);
        i++;
      }
      else {
        fprintf(stderr, "Missing number of iterations after -n.\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strcmp(argv[i], "-S") == 0) {
      if (i+1 < argc) {
        *screen_num = atoi(argv[i+1]);
        i++;
      }
      else {
        fprintf(stderr, "Missing screen number after -S.\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strcmp(argv[i], "--sticky") == 0) {
      options->sticky = 1;
    }

    if (strcmp(argv[i], "--timestamp") == 0) {
      options->show_timestamp = 1;
    }

    if (strcmp(argv[i], "-U") == 0) {
      options->show_user = 1;
    }

    if (strcmp(argv[i], "-u") == 0) {
      if (i+1 < argc) {
        if (isdigit(argv[i+1][0])) {
          options->watch_uid = atoi(argv[i+1]);
        }
        else {
          struct passwd* passwd = getpwnam(argv[i+1]);
          if (!passwd) {
            fprintf(stderr, "User name '%s' does not exist.\n", argv[i+1]);
            exit(EXIT_FAILURE);
          }
          options->watch_uid = passwd->pw_uid;
        }
        i++;
      }
      else {
        fprintf(stderr, "Missing user name after -u.\n");
        exit(EXIT_FAILURE);
      }
    }

    if (strcmp(argv[i], "-v") == 0) {
      print_version();
      exit(0);
    }

    if (strcmp(argv[i], "--version") == 0) {
      print_legal();
      exit(0);
    }

    if (strcmp(argv[i], "-w") == 0) {
      if (i+1 < argc) {
        options->watch_pid = atoi(argv[i+1]);
        if (options->watch_pid == 0)
          options->watch_name = strdup(argv[i+1]);
        i++;
      }
      else {
        fprintf(stderr, "Missing pid/name after -w.\n");
        exit(EXIT_FAILURE);
      }
    }
    fprintf(stderr, "Unknown flag: '%s'\n", argv[i]);
    exit(EXIT_FAILURE);
  }
}
