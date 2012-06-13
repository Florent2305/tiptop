/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2011, 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#include <config.h>

#if HAVE_LIBCURSES

#include <curses.h>
#include <string.h>

#include "helpwin.h"
#include "screen.h"


void target_dep_string(char*, int);


/* One line for screen column, 2 lines for the borders, 1 line for the
   target description. */
WINDOW* prepare_help_win(screen_t* screen)
{
  WINDOW* win;
  int n = screen->num_columns;

  /* in case we have more lines than rows on the display, limit the
     amount of text we print. */
  if (n+3 > LINES)
    n = LINES - 3;

  win = newwin(n+3, 70, LINES - n - 3, 5);
  return win;
}


void show_help_win(WINDOW* win, screen_t* screen)
{
  int  i, header_width = 0;
  char fmt[20] = { 0 };
  char msg[100] = { "Unknown" };
  int  n = screen->num_columns;

  /* in case we have more lines than rows on the display... */
  if (n+3 > LINES)
    n = LINES - 3;

  box(win, 0, 0);
  mvwprintw(win, 0, 10, " Help (h to close)");

  /* target-dependent message */
  target_dep_string(msg, sizeof(msg));

  if (has_colors())
    wattron(win, COLOR_PAIR(1));
  mvwprintw(win, 1, 1, "%s", msg);
  if (has_colors())
    wattroff(win, COLOR_PAIR(1));

  /* max size of column headers */
  for(i = 0; i < n; i++) {
    /* strip leading spaces, display looks better this way */
    char* ptr = screen->columns[i].header;
    while (*ptr == ' ')
      ptr++;
    if (strlen(ptr) > header_width)
      header_width = strlen(ptr);
  }
  /* generate sprintf format for headers */
  snprintf(fmt, sizeof(fmt) - 1, "%%-%ds: %%s", header_width);

  for(i = 0; i < n; i++) {
    /* strip leading spaces */
    char* ptr = screen->columns[i].header;
    while (*ptr == ' ')
      ptr++;
    mvwprintw(win, i+2, 1, fmt, ptr, screen->columns[i].description);
  }
  wrefresh(win);
}

#endif  /* HAVE_LIBCURSES */
