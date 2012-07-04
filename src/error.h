
#include <curses.h>

int close_error();
void error_printf(char* fmt, ...);

WINDOW* prepare_error_win(int nb_tids);
void show_error_win(WINDOW* win, int scroll);
int get_error();

void restart_error_win();
