#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "options.h"
#include "process.h"


static int pipefd[2];
static pid_t my_child = 0;


static void signal_handler(int sig)
{
  assert(sig == SIGALRM);
  update_name_cmdline(my_child);
}


/* Fork a new process, wait for the signal, and execute the command
   passed in parameter. */
void spawn(char** argv)
{
  pid_t child;
  struct sigaction action;

  action.sa_handler = signal_handler;
  sigaction(SIGALRM, &action, NULL);  /* prepare to receive timer signal */

  if (pipe(pipefd)) {  /* parent will signal through the pipe when ready */
    perror("pipe");
    exit(EXIT_FAILURE);
  }

  child = fork();
  if (child == -1) {
    perror("fork");
    exit(EXIT_FAILURE);
  }

  if (child == 0) {  /* in the child */
    char buffer;

    /* in case tiptop is set-UID root, we drop privileges in the child */
    if ((geteuid() == 0) && (getuid() != 0)) {
      int res = setuid(getuid());
      if (res != 0) {
        /* do not proceed as root */
        fprintf(stderr, "Could not create command\n");
        exit(EXIT_FAILURE);
      }
    }

    close(pipefd[1]);
    /* wait for parent to signal */
    read(pipefd[0], &buffer, 1);  /* blocking read */
    close(pipefd[0]);

    if (execvp(argv[0], argv) == -1) {
      perror("execvp");
      exit(EXIT_FAILURE);
    }
  }

  /* parent */
  close(pipefd[0]);
  my_child = child;
}


/* After the command has been forked, we update the list of
   processes. The newly created process will be discovered and
   hardware counters attached. We then signal the child to proceed
   with the execvp. */
void start_child()
{
  struct timeval tv1, tv2 = { 0 };
  struct itimerval it;

  write(pipefd[1], "!", 1);  /* will unblock the read */
  close(pipefd[1]);

  /* set timer, we need to update the name and command line of the new
     process in a little while */
  tv1.tv_sec = 0;
  tv1.tv_usec = 200000;  /* 200 ms */
  it.it_value = tv1;
  it.it_interval = tv2;  /* zero, no repeat */
  setitimer(ITIMER_REAL, &it, NULL);
}


void wait_for_child(pid_t pid, struct option* options)
{
  if (pid == my_child) {
    wait(NULL);  /* release system resources */
    options->command_done = 1;
  }
}
