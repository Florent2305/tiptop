/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2011 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#ifdef HAS_CURSES

#ifndef _HELPWIN_H
#define _HELPWIN_H

#include <curses.h>
#include "screen.h"

WINDOW* prepare_help_win(screen_t*);
void show_help_win(WINDOW*, screen_t*);


#endif  /* _HELPWIN_H */

#endif  /* HAS_CURSES */
