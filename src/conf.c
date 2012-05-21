/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2011 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conf.h"
#include "options.h"

#define LINE_SIZE 100

static const char* const config_file = ".tiptoprc";

/* Read configuration file. First check the TIPTOP environment
   variable, then the local directory, finally $HOME. */
int read_config(struct option* options)
{
  /*
    TODO : Argument determinant l'adresse du fichier de conf!
  */

  char* path = NULL;
  FILE* f = NULL;

  /*
  *Variable environnement $TIPTOP
  */
  char* val = getenv("TIPTOP");
  if (val) {
    path = strdup(val);
  }
  else {
    /*
     *Dossier courant
     */
    f = fopen(config_file, "r");
    if (f != NULL) {
      path = "";
      fclose(f);
    }
    else{  
      /*
       * Dans le home
       */
      char* home = getenv("HOME");
      if (home){
	path = strdup(home);
      }
    } 
  }

  if(path != NULL){
    char* file;

    if(strcmp(path,"") != 0 ){
       file = malloc(strlen(path)+strlen(config_file)+2);
       sprintf(file,"%s/%s",path,config_file);
       free(path);
    }
    else file = strdup(config_file);
 
    int res = parse_doc(file, options );      
    free(file);
    return res;	
  }
  else return -1;
  return -1;
}
