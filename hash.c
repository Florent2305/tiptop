/* Minimalist hash table to keep track of 'struct process'
   entries. The key is the thread ID 'tid'.

   When the list of processes is realloc'd and the data actually moved
   in memory, the hash table must be completely deleted and
   rebuilt. This is costly, but should be rather rare.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "hash.h"

#define NUM_HASH_KEYS 16  /* must be a power of 2 */

struct hash_entry {
  struct process* data;
  struct hash_entry* next;
};

static struct hash_entry* hash_map[NUM_HASH_KEYS];


/* simplest possible hash function */
static inline int hash(int x)
{
  return x & (NUM_HASH_KEYS - 1);
}


/* Just initialize the buckets to NULL */
void hash_init()
{
  int i;
  assert((NUM_HASH_KEYS & (NUM_HASH_KEYS - 1)) == 0);  /* power of 2 */
  for(i=0; i < NUM_HASH_KEYS; i++)
    hash_map[i] = NULL;
}


/* Deallocate all entries, and reset the buckets to NULL. */
void hash_fini()
{
  int i;
  for(i=0; i < NUM_HASH_KEYS; i++) {
    struct hash_entry* ptr = hash_map[i];
    while (ptr) {
      struct hash_entry* next = ptr->next;
      free(ptr);
      ptr = next;
    }
    hash_map[i] = NULL;
  }
}


/* Dump all entries (skip NULL buckets). */
void hash_dump()
{
  int i;
  printf("---------------\n");
  for(i=0; i < NUM_HASH_KEYS; i++) {
    struct hash_entry* ptr = hash_map[i];
    if (ptr)
      printf("[%3d] ", i);
    while(ptr) {
      printf("%d ", ptr->data->tid);
      ptr = ptr->next;
    }
    printf("\n");
  }
}


/* Add a pair (key, process) to the table. If the key is already
   present in the table, nothing happens. */
void hash_add(int key, struct process* proc)
{
  struct hash_entry* new;
  int h = hash(key);
  struct hash_entry* ptr = hash_map[h];

  while (ptr) {
    if (ptr->data->tid == key)  /* already in */
      return;
    ptr = ptr->next;
  }
  /* not found, insert in front */
  new = malloc(sizeof(struct hash_entry));
  new->data = proc;
  new->next = hash_map[h];  /* next is current first element (from bucket) */
  hash_map[h] = new;  /* bucket now points to new element */
}


/* Retrieve a process from the key */
struct process* hash_get(int key)
{
  int h = hash(key);
  struct hash_entry* ptr = hash_map[h];

  while (ptr) {
    if (ptr->data->tid == key)  /* found */
      return ptr->data;
    ptr = ptr->next;
  }
  return NULL;  /* not found */
}


/* Add all processes from 'list' to the map. This is useful when the
   list has been moved because of a realloc, and entries must be
   fixed. */
void hash_fillin(struct process_list* list)
{
  int i;
  struct process* const pl = list->processes;
  int num_tids = list->num_tids;

  for(i=0; i < num_tids; i++)
    hash_add(pl[i].tid, &pl[i]);
}
