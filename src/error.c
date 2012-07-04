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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "errno.h"
#include "screen.h"

#define ERROR_MAX 32
#define LENGHT 1000

static const char* const error_file_name = "tiptop.error";
static FILE* error_file = NULL;

static int nb_error = 0;

int get_error()
{
  return nb_error;
}
void close_error(){
  if(error_file)
    fclose(error_file);
}

void error_printf(char* fmt, ...)
{
  va_list args;
  nb_error++;
  /* open file the first time we get here. */
  if (!error_file) {
    error_file = fopen(error_file_name, "w+");
    if (!error_file) {
      perror("fopen");
      fprintf(stderr, "Cannot open error file.\n");
      exit(EXIT_FAILURE);
    }
  }
  va_start(args, fmt);
  vfprintf(error_file, fmt, args);
}


int boot = 0;
int tids = 0;

WINDOW* prepare_error_win(int nb_tids)
{
  WINDOW* we;
  /* Keep dimension of the box */
  if(boot == 0){
    boot++;
    tids = nb_tids;
  }
    
  int l = LINES-tids-5;
  /* Encure a place for window */
  if(l <= 0) 
    l = 10;
  
  we = newwin(l, COLS, LINES-l,0);
  // clearok(we, TRUE);

  return we;
}


void show_error_win(WINDOW* win, int scroll, int nb_proc){

  long current_pos;
  int maxx , maxy , i;

  if(!error_file)
    return;

  if(!win)
    win = prepare_error_win(nb_proc);

  getmaxyx(win, maxy, maxx);

  char buf[maxx];
  char blank[maxx-3];
  int pos=0;

  
  for(i=0;i<maxx-3;i++) 
    blank[i] = ' ';

  /* Save currrent position in tiptop.error */
  current_pos = ftell(error_file);
  rewind(error_file);

  box(win, 0, 0);
  mvwprintw(win, 0, 5, " In .tiptoprc: %d errors detected (e to close) ",
            nb_error);

  pos++;

  /* scrolling the file */
  for(i=0;
      i<scroll && fgets(buf, maxx-2, error_file) != NULL && i<(nb_error-1);
      i++)
  {
    if(buf[strlen(buf)-1] != '\n')
      i--;
  }

  if(i == 0)
    mvwprintw(win,pos++,1,"BEGIN", buf);
  else
    mvwprintw(win,pos++,1,"......", buf);

  while (fgets(buf, maxx-2, error_file) != NULL && pos<maxy-2) {

    if (buf[strlen(buf-1)] == '\n') /* To keep box's border */
      buf[strlen(buf-1)] = 0;

    mvwprintw(win,pos++,1,"%s", buf);
  }
  
  if (pos != maxy-2) { /* To complete screen */
    while(pos < maxy-2)
      mvwprintw(win,pos++,1,"%s",blank);
    mvwprintw(win,pos,1,"END", i);
  }
  else
    mvwprintw(win,pos,1,"......", i);


  /* restoring older state of tiptop.error */
  fseek(error_file, current_pos, SEEK_SET);
  wrefresh(win);
}

void restart_error_win()
{
  boot = 0;
}
