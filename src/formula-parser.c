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
 *
 *
 *                  ***
 *
 *          Parser's Grammar Rules
 *
 *                  ***
 *
 *** Expr      => SimlExpr FollExpr
 *
 *** SimlExpr  => Elem FollElem
 *
 *** Elem      => Unit
 *             => ( Expr )
 *             => delta( Unit )
 *
 *** FollElem  => OP2 Expr
 *             => NULL
 *
 *** FollExpr  => OP1 Expr
 *             => NULL
 *
 *** OP1       =>  '+' | '-' | '<' | '>' | '&' | '|'
 *
 *** OP2       =>  / | *
 *
 *** Unit      => Counters...(every symbol which isn't a separator)
 *             => Constants..{0...9}
 *
 *
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "formula-parser.h"
#include "utils-expression.h"

static unit* Unit();
static operation* FollowingElement();
static expression* Element();
static operation* FollowingExpression();
static expression* SimpleExpression();
static token_t get_next_token(void);
static char* get_current_word(void);

/* Current analyzed token */
static token_t cur_tok = END;

/* String of the analyzed expression */
static char* txt;

/* Index in string "txt" where begin the current token*/
static int here = 0;

/* Index in string "txt" where the next token start(end of the current) */
static int next_tok=0;


/* Some macros to analyse string */

#define is_sep(c) (c=='(' || c==')' || c=='<' || c=='>' || c=='+'   || c=='/'   || c=='-'  || c=='*' || c == ' ' || c == '|')

#define is_op1(c) (c=='+' || c=='-')
#define is_op2(c) (c=='/'|| c=='*')

#define is_number(c) ((c >= '0' && c <= '9') || c == '.')
#define is_dot(c) (c == '.' || c == ',')
#define is_char(c) (c >= 'a' && c <= 'z' && c >= 'A' && c <= 'Z')

/* Return string representation of current token in "txt" */

char* get_current_word()
{
  return strndup(&txt[here], next_tok - here);
}

/* Look for next token and place it in global "cur_token" */

token_t get_next_token ()
{
  char car;
  int only_numb = 0;
  int dot = 0;
  int len_txt = strlen(txt);

  if(next_tok == len_txt) {
    cur_tok = END;
    goto end;
  }

  while(next_tok<len_txt && isspace(txt[next_tok]))
    next_tok++;

  here = next_tok;
  car = txt[here];

  if (is_sep(car)) {
    next_tok++;
    if (car == '+') cur_tok =  ADD;
    if (car == '-') cur_tok =  SUB;
    if (car == '*') cur_tok =  MUL;
    if (car == '/') cur_tok =  DIV;
    if (car == ')') cur_tok =  PD;
    if (car == '(') cur_tok =  PG;
    if (car == '|') cur_tok =  OR;
    goto end;
  }

  while ((next_tok < len_txt) && (!is_sep(car))) {
    if (!is_number(car))
      only_numb = -1;
    if(is_dot(car))
      dot++;
    car = txt[++next_tok];
  }

  if(only_numb == 0 && dot <= 1) {
    cur_tok =  NUMBER;
    goto end;
  }
  if(strncmp("delta(", &txt[here], 6) == 0){
    next_tok ++;
    cur_tok = DELTA;
    goto end;
  }
  if(strncmp("and", &txt[here], 3) == 0){
    cur_tok = AND;
    goto end;
  }
  if(strncmp("shl", &txt[here], 3) == 0){
    cur_tok = SHL;
    goto end;
  }
  if(strncmp("shr", &txt[here], 3) == 0){
    cur_tok = SHR;
    goto end;
  }
  if(strncmp("or", &txt[here], 2) == 0){
    cur_tok = OR;
    goto end;
  }
  cur_tok = COUNTER;

 end:
  return cur_tok;
}

/*  Beginning's parser  */

static char Op1()
{
  char op='e';
  if(cur_tok == ADD)
    op='+';
  if(cur_tok == SUB)
    op='-';
  if(cur_tok == SHR)
    op='>';
  if(cur_tok == SHL)
    op='<';
  if(cur_tok == AND)
    op='&';
  if(cur_tok == OR)
    op='|';

  get_next_token();
  return op;
}

static char Op2()
{
  char op='e';
  if(cur_tok == MUL)
     op='*';
  if(cur_tok == DIV)
    op='/';
  get_next_token();
  return op;
 }


expression* Expression(char* t, int init)
{
  expression* e = NULL;
  expression* res = alloc_expression();

  debug_printf("[Formula-parser]Expression");

  if(init == 1) {
    here=0;
    next_tok=0;
    txt = t;
    get_next_token();
  }
  if (cur_tok == PG || cur_tok == DELTA || cur_tok == COUNTER || cur_tok == NUMBER)
  {
    e = SimpleExpression();     /* Going in Simple Expression */

    if (e->type == ERROR) {
      free_expression(res);
      return e;
    }
    if (cur_tok == ADD || cur_tok == SUB || cur_tok == SHR || cur_tok == SHL ||
        cur_tok == AND || cur_tok == OR)
    {
      res->op = FollowingExpression();       /* Going in FollowingExpression */

      if (res->op->operateur == ERROR) {
        free_expression(e);
        goto error;
      }
      res->type = OPER;
      res->op->exp1 = e;
    }
    if (cur_tok == END || cur_tok == PD) {
      /* Just a EimpleExpression (FollowExpr == NULL) */
      if (res->op) {
        return res;
      }
      else {
        free_expression(res);
        return e;
      }
    }
    else free_expression(e);
  }

 error:
  res->type = ERROR;
  return res;
}


static expression* SimpleExpression()
{
  expression* ex1 = NULL;
  expression* ex2 = NULL ;

 debug_printf("[Formula-parser]SimpleExpression");

 if (cur_tok == PG || cur_tok == DELTA || cur_tok == COUNTER || cur_tok == NUMBER)
 {
   ex1 = Element();  /* Going in Element */

   if (ex1->type != ERROR) {
     if(cur_tok == MUL || cur_tok == DIV) {
       ex2=alloc_expression();
       ex2->op = FollowingElement();       /* Going in Following Element */

       if (ex2->op->operateur == ERROR) { /* Syntax error */
         free_expression(ex1);
         ex2->type = ERROR;
       }
       else {
         ex2->type = OPER;
         ex2->op->exp1 = ex1;
       }
       return ex2;
     }
   }
   return ex1;
 }

  ex2=alloc_expression();
  ex2->type = ERROR;
  return ex2;
}

static expression* Element()
{
  expression* ex3 = NULL;
  int delta = 0;

  debug_printf("[Formula-parser]Element\n");

  if (cur_tok == PG) {
    get_next_token();

    if (cur_tok == PG || cur_tok == DELTA || cur_tok == COUNTER || cur_tok == NUMBER)
    {
      ex3 = Expression(NULL,0);    /* => Expression */

      if (cur_tok != PD || ex3->type == ERROR)
        ex3->type = ERROR;
      else
        get_next_token();
      return ex3;
    }
    ex3 = alloc_expression();
    goto error;
  }
  else {
    ex3 = alloc_expression();

    if (cur_tok == DELTA) {
      get_next_token();
      delta = 1;
    }
    if (cur_tok == NUMBER || cur_tok == COUNTER) {
      ex3->ele = Unit();  /* => Unit */
      if (delta == 1) {
        if (cur_tok != PD) /* Syntax error */
          goto error;
        get_next_token();
        ex3->ele->delta = DELTA;
      }
      ex3->type = ELEM;
      return ex3;
    }
  }
 error:
  ex3->type = ERROR;
  return ex3;
}


static operation* FollowingExpression()
{
  char c;
  operation* op = NULL;
  op =alloc_operation();
  debug_printf("[Formula-parser]FollowingExpression");

  c = Op1();   /* => Op1 */

  if (cur_tok == PG || cur_tok == DELTA || cur_tok == COUNTER || cur_tok == NUMBER)
    {
      op->exp2 = Expression(NULL, 0);     /* => Expression */

      if (op->exp2->type != ERROR) {
        op->operateur = c;
        return op;
      }
    }
  /* Syntax Error */
  op->operateur = ERROR;
  return op;
}


static operation* FollowingElement()
{
  operation* op = NULL;
  char c;
  op = alloc_operation();

  c = Op2(); /* => Op2 */

  if (cur_tok == PG || cur_tok == DELTA || cur_tok == COUNTER || cur_tok == NUMBER)
    {
      op->exp2 = SimpleExpression();   /* => SimpleExpression*/

      if (op->exp2->type != ERROR) {
        op->operateur = c;
        return op;
      }
    }
  /* Syntax Error */
  op->operateur = ERROR;
  return op;
}


static unit* Unit()
{
  static unit* u = NULL;
  char* w = get_current_word();
  u = alloc_unit();

  if (cur_tok == NUMBER)
    {
      u->type = CONST;
      u->val = atof(w);
    }
  else if (cur_tok == COUNTER)
    {
      u->type = COUNT;
      u->alias = strdup(w);;
    }
  free(w);
  get_next_token();
  return u;
}
