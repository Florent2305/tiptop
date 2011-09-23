#ifndef _SPAWN_H
#define _SPAWN_H

#include <sys/types.h>

#include "options.h"

void spawn(char** argv);
void start_child();
void wait_for_child(pid_t pid, struct option* options);

#endif  /* _SPAWN_H */