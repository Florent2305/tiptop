/*
 * This file is part of tiptop.
 *
 * Author: Antoine NAUDIN
 * Copyright (c) 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#include <curses.h>

void close_error();
void error_printf(char* fmt, ...);

WINDOW* prepare_error_win(int nb_tids);
void show_error_win(WINDOW* win, int scroll, int nb_proc);
int get_error();

void restart_error_win();
