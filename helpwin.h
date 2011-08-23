#ifndef _HELPWIN_H
#define _HELPWIN_H

#include <curses.h>
#include "screen.h"

WINDOW* prepare_help_win(screen_t*);
void show_help_win(WINDOW*, screen_t*);


#endif  /* _HELPWIN_H */
