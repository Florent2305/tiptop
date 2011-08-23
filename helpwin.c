#ifdef HAS_CURSES

#include <curses.h>
#include <string.h>
#include "screen.h"

WINDOW* prepare_help_win(screen_t* screen)
{
  WINDOW* win;
  int n = screen->num_columns;

  /* in case we have more lines than rows on the display, limit the
     amount of text we print. */
  if (n+2 > LINES)
    n = LINES - 2;

  win = newwin(n+2, 70, LINES - n - 2, 5);
  return win;
}


void show_help_win(WINDOW* win, screen_t* screen)
{
  int  i, header_width = 0;
  char fmt[20];
  int  n = screen->num_columns;

  /* in case we have more lines than rows on the display... */
  if (n+2 > LINES)
    n = LINES - 2;

  box(win, 0, 0);
  wmove(win, 0, 10);
  wprintw(win, " Help (h to close)");
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
  sprintf(fmt, "%%-%ds: %%s", header_width);
  for(i = 0; i < n; i++) {
    /* strip leading spaces */
    char* ptr = screen->columns[i].header;
    while (*ptr == ' ')
      ptr++;
    wmove(win, i+1, 1);
    wprintw(win, fmt, ptr, screen->columns[i].description);
  }
  wrefresh(win);
}

#endif  /* HAS_CURSES */
