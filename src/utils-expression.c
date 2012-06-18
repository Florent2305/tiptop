/*
 * This file is part of tiptop.
 *
 * Author: Antoine NAUDIN
 * Copyright (c) 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formula-parser.h"
#include "process.h"
#include "screen.h"
#include "utils-expression.h"


/* dedicated destroyer to free expressions */

void free_unit(unit* u)
{
  if (u == NULL)
    return;
  if (u->alias != NULL)
    free(u->alias);
  free(u);
}


void free_expression (expression* e)
{
  if (e == NULL) {
    printf("expression NULL!\n");
    return;
  }
  free_unit(e->ele);
  free_operation(e->op);
  free(e);
}


void free_operation(operation* p)
{
  if (p == NULL)
    return;
  free_expression(p->exp1);
  free_expression(p->exp2);
  free(p);
}


/* Print an expression */
void parcours_expression(expression* e)
{
  if (e->type == ELEM){
    if (e->ele->type == COUNT) {
      if (e->ele->delta == DELTA)
        printf("delta(%s)", e->ele->alias);
      else
        printf("%s",e->ele->alias);
    }
    else if (e->ele->type == CONST) {
      printf("%f", e->ele->val);
    }
  }
  else if (e->type == OPER && e->op != NULL) {
    printf("(");
    parcours_expression(e->op->exp1);
    printf(" %c ",e->op->operateur);
    parcours_expression(e->op->exp2);
    printf(")");
  }
}


/* Build a representative stream of an expression */
int build_expression(expression* e, FILE* fd)
{
  if (e->type == ELEM) {
    if (e->ele->type == COUNT) {
      if (e->ele->delta == DELTA)
        return fprintf(fd, "delta(%s)", e->ele->alias);
      else
        return fprintf(fd, "%s", e->ele->alias);
    }
    else if (e->ele->type == CONST)
      return fprintf(fd, "%lf", e->ele->val);
  }
  else if (e->type == OPER && e->op != NULL) {
    if (fprintf(fd, "(") < 0)
      return -1;
    if (build_expression(e->op->exp1, fd) < 0)
      return -1;

    if (e->op->operateur == '<') {
      if (fprintf(fd, " &lt; ") < 0)
        return -1;
    }
    else if (e->op->operateur == '>') {
      if (fprintf(fd, " &gt; ") < 0)
        return -1;
    }
    else if (e->op->operateur == '&') {
      if (fprintf(fd, " &amp; ") < 0)
        return -1;
    }
    else if (fprintf(fd, " %c ", e->op->operateur) < 0)
      return -1;

    if (build_expression(e->op->exp2, fd) < 0)
      return -1;
    if (fprintf(fd, ")") < 0)
      return -1;
  }
  return 0;
}


/* Remove every kind of NOP char: space tabulation and new ligne*/
static char* remove_space(char* txt)
{
  int i,nb=0, j=0, lg = strlen(txt);
  char* res = malloc(lg+1);
  for(i=0 ; i < lg; i++){
    if (isspace(txt[i]))
      nb++;
    else
      res[j++] = txt[i];
  }
  res[j] = '\0';
  return res;
}


/* Parse and return a representative tree */
expression* parser_expression (char* txt)
{
   expression* expr= NULL;
   char* clean = NULL;
   clean = remove_space(txt);
   expr = Expression(clean, 0);
   free(clean);
   return expr;
}


/* Tools to find counters's ID */
static int get_counter_id(char* alias, counter_t* tab, int nbc)
{
  int i;
  for(i=0 ;i<nbc; i++)
    if (strcmp(alias, tab[i].alias) == 0)
      return i;
  return -1;
}


/* Tools to get counter value */
static double get_counter_value(unit* e, counter_t* tab, int nbc, char delta,
                                struct process* p, int* error)
{
  int id;
  if (strcmp(e->alias, "CPU_TOT") == 0)
    return p->cpu_percent;

  if (strcmp(e->alias, "CPU_SYS") == 0)
    return p->cpu_percent_s;

  if (strcmp(e->alias, "CPU_USER") == 0)
    return p->cpu_percent_u;

  if (strcmp(e->alias, "PROC_ID") == 0) {
    if (p->proc_id != -1) {
      return (double)p->proc_id;
    }
    else {
      *error=1;
      return 1;
    }
  }
  id = get_counter_id(e->alias, tab, nbc);
  if ((id == -1) || (p->values[id] == 0xffffffff)) {
    *error = 1;
    return 1;
  }

  if (delta == DELTA)
    return (double) (p->values[id] - p->prev_values[id]);

  return (double) p->values[id];
}


/* Calculous and return result of expression gived in entry
 *
 * (argument error is used for indicate and determine which kind of
 * error appear during evaulation:
 *      1 => Invalid Counter
 *      2 => Divide per zero (also if it is double)
 */
double evaluate_column_expression(expression* e, counter_t* c, int nbc,
                           struct process* p, int* error)
{
  if (e == NULL) {
    *error = 1;
    return 0;
  }
  *error = 0;
  if (e->type == ELEM) {
    if (e->ele->type == COUNT)
      return get_counter_value(e->ele, c, nbc, e->ele->delta, p, error);
    else if(e->ele->type == CONST)
      return e->ele->val;
  }
  else if ((e->type == OPER) && (e->op != NULL)) {

    switch(e->op->operateur) {
    case '+':
      return evaluate_column_expression(e->op->exp1, c, nbc, p, error) +
             evaluate_column_expression(e->op->exp2, c, nbc, p, error);
      break;

    case '-':
      return evaluate_column_expression(e->op->exp1, c, nbc, p, error) -
             evaluate_column_expression(e->op->exp2, c, nbc, p, error);
      break;

    case '*':
      return evaluate_column_expression(e->op->exp1, c, nbc, p, error) *
             evaluate_column_expression(e->op->exp2, c, nbc, p, error);
      break;
    case '/': {
      double tmp = evaluate_column_expression(e->op->exp2, c, nbc, p, error);
      if (tmp == 0) {
        *error = 2;
        return 0;
      }
      return evaluate_column_expression(e->op->exp1, c, nbc, p, error) / tmp;
      break;
    }
    default:
      assert(0);
    }
  }
  return 0;
}


uint64_t evaluate_counter_expression(expression* e, int* error)
{
  uint64_t val = 0;
  if (e == NULL  || e->type == ERROR) {
    (*error)++;
    return 0;
  }
  if (e->type == ELEM) {
    if (e->ele->type == COUNT){
      if (get_counter_config(e->ele->alias, &val) < 0)
        (*error)++;
      return val;
    }
    else return (uint64_t) e->ele->val;
  }
  else if ((e->type == OPER) && (e->op != NULL)) {
    switch(e->op->operateur) {
    case '<':
      return evaluate_counter_expression(e->op->exp1, error) <<
	evaluate_counter_expression(e->op->exp2, error);
      break;
    case '>':
      return evaluate_counter_expression(e->op->exp1, error) >>
	evaluate_counter_expression(e->op->exp2, error);
      break;
    case '&':
      return evaluate_counter_expression(e->op->exp1, error) &
	evaluate_counter_expression(e->op->exp2, error);
      break;
    case '|':
      return evaluate_counter_expression(e->op->exp1, error) |
	evaluate_counter_expression(e->op->exp2, error);
      break;
    default:
      assert(0);
    }
  }
  return 0;
}
