#ifndef _UTILS_H
#define _UTILS_H

/* string utilities */
char* str_init(const char* str, int* curr_alloc);
char* str_append(char* first, int* curr_alloc, const char* const second);


#endif  /* _UTILS_H */
