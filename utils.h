/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2011 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#ifndef _UTILS_H
#define _UTILS_H

/* string utilities */
char* str_init(const char* str, int* curr_alloc);
char* str_append(char* first, int* curr_alloc, const char* const second);


#endif  /* _UTILS_H */
