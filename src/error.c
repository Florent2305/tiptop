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
#include <unistd.h>

#include "errno.h"
#include "screen.h"

static char* path_error_file = NULL;
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
void set_path_error(char* path){
  path_error_file = path;
}


void error_printf(char* fmt, ...)
{
  va_list args;
  nb_error++;

  /* open file the first time we get here. */
  if (!error_file) {

    if(path_error_file){
      error_file = fopen(path_error_file, "w+");
      if (!error_file) {
	perror("fopen");
	fprintf(stderr, "Cannot open error file.\n");
	exit(EXIT_FAILURE);
      }   
    }
    else{
      error_file = tmpfile();
      if(error_file == NULL){
	fprintf(stderr, "Cannot open error file.\n");
	exit(EXIT_FAILURE);
      }
    }
  }
  va_start(args, fmt);
  vfprintf(error_file, fmt, args);
}


WINDOW* prepare_error_win(int nb_tids)
{
  WINDOW* we;
    
  int l = LINES-nb_tids-5;
  /* Encure a place for window */
  if(l <= 0) 
    l = 10;
  
  we = newwin(l, COLS, LINES-l,0);
  return we;
}
static int nbp = 0;

void show_error_win(WINDOW* win, int scroll, int nb_proc){

  long current_pos;
  int maxx , maxy , i, pos=1;
  int tmp;
  if(!error_file)
    return;

  if(nb_proc != -1)
    nbp = nb_proc;

  if(!win)
    win = prepare_error_win(nbp);

  getmaxyx(win, maxy, maxx);

  char buf[maxx];
  char blank[maxx-3];
  
  for(i=0;i<maxx-3;i++) 
    blank[i] = ' ';

  /* Save currrent position in tiptop.error */
  current_pos = ftell(error_file);
  rewind(error_file);

  box(win, 0, 0);
  mvwprintw(win, 0, 5, " %d errors detected (e to close) ",
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
