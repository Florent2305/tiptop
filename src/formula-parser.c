/*
 * This file is part of tiptop.
 *
 * Author: Antoine NAUDIN
 * Copyright (c) 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */


/*
 * Mathematical Expression Parser:
 *
 * - Operators accepted :  +, -, /, *, delta(), (, )
 *
 * 	delta(Counter) : calculous difference in a time interval
 * 	(option delay)
 *
 * - Used Variables : integer, double or string (as couters's alias
 *   declared in screen configuration)
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "formula-parser.h"
#include "utils-expression.h"

static expression* alloc_expression();
static unit* Unit(char* txt);
static operation* FollowingElement(char* txt);
static expression* Element(char* txt);
static operation* FollowingExpression(char* txt);
static expression* SimpleExpression(char* txt);

static int here = 0;


#define is_sep(c) (c=='(' || c==')'   || c=='+'   || c=='/'   || c=='-'  || c=='*'   || c=='<'   || c=='>'  || c == '&'   || c == '|')

#define is_op1(c) (c=='+' || c=='-'|| c=='<'   || c=='>'  || c == '&'   || c == '|' )
#define is_op2(c) (c=='/'|| c=='*')

#define is_number(c) (c >= '0' && c <= '9')
#define is_dot(c) (c == '.' || c == ',')
#define is_char(c) (c >= 'a' && c <= 'z' && c >= 'A' && c <= 'Z')



static int number_is(char* txt)
{
  int lg = strlen(txt);
  int i=0, dot=0;
  for(i=0; i<lg; i++) {
    if(is_dot(txt[i])) dot++;
    else if(!is_number(txt[i]))
      return -1;
  }
  if(dot <= 1)
    return 0;
  else
    return -1;
}


static int is_unit(char* txt)
{
  int lg = strlen(txt);
  int i=0;
  for(i=0; i<lg; i++)
    if (is_sep(txt[i]))
      return -1;
  return 0;
}


static int lookfor_next_word(char* txt, int now)
{
  int len = strlen(txt);
  if (now >= len)
    return -1;
  if (is_sep(txt[now])){
      return now;
  }
  else
    return lookfor_next_word(txt, now+1);
}


static char* get_next_word(char* buf)
{
  int   lg, res;
  char* buf2;

  if (buf == NULL)
    return "";

  lg = strlen(buf);
  if (lg == 0)
    return "";

  res = lookfor_next_word(buf, here);
  if (res == -1)
    res = lg;

  if (res == here)
    res++;  /* Si le premier char est un separateur */

  buf2 = malloc(sizeof(char)*(res-here)+1);
  strncpy(buf2, &buf[here], res-here);
  buf2[res-here] = 0;
  return buf2;
}


static unit* alloc_unit()
{
  unit* u = malloc(sizeof(unit));
  u->alias = NULL;
  u->val = -1;
  u->type = -1;
  u->delta = -1;
  return u;
}


static expression* alloc_expression()
{
  expression* e = malloc(sizeof(expression));
  e->ele = NULL;
  e->op = NULL;
  e->type = -1;
  return e;
}


static operation* alloc_operation()
{
  operation* o = malloc(sizeof(operation));
  o->operateur = '!';
  o->exp1 = NULL;
  o->exp2 = NULL;
  return o;
}


/*
*********************************
*                               *
*    Parser's Grammar Rules     *
*                               *
*********************************
 *
 *Expr => SimlExpr FollExpr
 *
 *SimlExpr => Elem FollElem
 *
 *Elem => Unit
 *     => ( Expr )
 *     => delta( Unit )
 *
 *FollElem => OP1 Expr
 *         => NULL
 *
 *FollExpr => OP2 Exp
 *         => NULL
 *
 *OP1 =>  '+' | '-' | '<' | '>' | '&' | '|'
 *
 *OP2 =>  / | *
 *
 *Unit => Counters...
 *     => Constants
 *
 */

expression* Expression(char* txt, int init)
{
  char*       buf;
  expression* e = NULL;
  operation* se = NULL;
  expression* res;

  debug_printf("[Formula-parser]Expression: %s", &txt[here]);


  res = alloc_expression();


  if (init == 0)
    here = 0;
  if (txt == NULL)
    return NULL;
  
  buf = get_next_word(txt);
  if (buf[0] == '(' || strcmp(buf, "delta") == 0 || is_unit(buf) == 0 ||
      strlen(buf) == 0)
  {
    free(buf);

    e = SimpleExpression(txt);

    if (e->type == ERROR)
      return e;

    se = FollowingExpression(txt);


    if (se != NULL && se->operateur == ERROR) {
      free_expression(e);
      res->type = ERROR;
      return res;
    }

    if (se != NULL) {
      res->type = OPER;
      res->op = se;
      res->op->exp1 = e;
      return res;
    }
    else {
      if (e->type == ELEM) {
        res->type = ELEM;
        res->ele = e->ele;
      }
      else {
        res->type = OPER;
        res->op = e->op;
      }
      free(e);
      return res;
    }
  }

  free(buf);
  res = alloc_expression();
  res->type = ERROR;
  return res;
}


static expression* SimpleExpression(char* txt)
{
  expression* tmp = NULL;
  operation* op = NULL;
  expression* exp;

  debug_printf("[Formula-parser]SimpleExpression: %s", &txt[here]);

  if (strlen(&txt[here]) == 0)
    return NULL;

  tmp = Element(txt);

  if (tmp->type == ERROR)
    return tmp;

  op = FollowingElement(txt);

  exp = alloc_expression();

  if (op != NULL && op->operateur == ERROR) {
    if (op != NULL)
      free_operation(op);
    free_expression(tmp);
    exp->type = ERROR;
    return exp;
  }
  if (op != NULL) {
    exp->type = OPER;
    exp->op = op;
    exp->op->exp1 = tmp;
    return exp;
  }
  free(exp);
  return tmp;
}


static operation* FollowingExpression(char* txt)
{
  char* buf = NULL;
  int len = strlen(txt);
  buf = get_next_word(txt);
  debug_printf("[Formula-parser]FollowingExpression: %s\n",buf);

  if (here < len && is_op1(buf[0])) {
      operation *op = alloc_operation();
      op->exp2 = NULL;
      here++;
      op->operateur = buf[0];
      free(buf);
      op->exp2 = Expression(txt, 1);
      if (op->exp2 == NULL || op->exp2->type == ERROR) {
        op->operateur = ERROR;
      }
      return op;
    }
  else
    if (buf)
      free(buf);
  return NULL;
}


static expression* Element(char* txt)
{
  expression* exp = NULL;
  unit* un = NULL;
  int delta = 0;

  if (txt[here] == '(')
    {  /* parenthese */
      debug_printf("[Formula-parser]Elem parenthesis\n");
      here++;
      exp = Expression(txt, 1);
      if (txt[here] != ')' || exp->type == ERROR) {
        if (!exp)
          exp = alloc_expression();
        goto error;
      }
      else
        here++;
    }
  else if (!is_sep(txt[here])) {
      char* buf;

      debug_printf("[Formula-parser]Elem ");
      buf = get_next_word(txt);

      if (strcmp(buf, "delta") == 0) {  /* ratio */
        debug_printf("[Formula-parser] delta\n");
        delta = 1;
        here += 6 ;
        if (txt[here-1] != '(') {
          exp = alloc_expression();
          free(buf);
          goto error;
        }
      }
      free(buf);
      exp = alloc_expression();

      un = Unit(txt);

      if(un->type == ERROR){
        free_unit(un);
        goto error;
      }

      if (delta == 1) {

        if (txt[here] != ')') {
          free_unit(un);
          goto error;
        }
        un->delta = ERROR;
        here++;
      }
      exp->type = ELEM;
      exp->ele = un;
    }
  return exp;

 error:
  exp->type = ERROR;
  return exp;
}


static operation* FollowingElement(char* txt)
{
  operation* op = NULL;
  expression* exp = NULL;
  char ope;

  if (is_op2(txt[here])) {
    ope = txt[here];
    here++;
    exp = SimpleExpression(txt);

    op = alloc_operation();

    if (exp == NULL || exp->type == ERROR) {
      /* Syntaxe Error */
      free_expression(exp);
      op->operateur = ERROR;
      return op;
    }

    op->exp2 = exp;
    op->operateur = ope;
    return op;
  }
  return NULL;
}


static unit* Unit(char* txt)
{
  char* buf;
  unit* res;

  debug_printf("[Formula-parser]Unit: %s\n", &txt[here]);

  buf = get_next_word(txt);
  res = alloc_unit();

  if (number_is(buf) == 0) {
    res->type = CONST;
    res->val = atof(buf);
    here += strlen(buf);
    free(buf);
    return res;
  }
  else if (is_unit(buf) == 0) {
    res->type = COUNT;
    
    res->alias = strdup(buf);
    here += strlen(buf);
    free(buf);
    return res;
  }
  else {
    res->type = ERROR;
    free(buf);
    return res;
  }
}
