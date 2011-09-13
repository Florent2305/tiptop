#ifndef _HASH_H
#define _HASH_H

#include "process.h"


void hash_init();
void hash_fini();
void hash_add(int key, struct process* proc);
struct process* hash_get(int key);
void hash_del(int key);

#endif  /* _HASH_H */
