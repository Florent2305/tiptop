/*
 * This file is part of tiptop.
 *
 * Author: Antoine NAUDIN
 * Copyright (c) 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#ifndef _FORMULA_PARSER_H
#define _FORMULA_PARSER_H


typedef enum{
  COUNT,
  CONST,
  OPER,
  ELEM,
  DELT,
  ERROR
} type_t;

/*
char* str_tok[]={
  "ADD",
  "SUB",
  "MUL",
  "DIV",
  "AND",
  "OR",
  "PD",
  "PG",
  "CD",
  "CG",
  "COUNTER",
  "NUMBER",
  "DELTA",
  "END"
} ;
*/

typedef enum {
  ADD,
  SUB,
  MUL,
  DIV,
  AND,
  OR,
  PD,
  PG,
  CD,
  CG,
  COUNTER,
  NUMBER,
  DELTA,
  END
} token_t ;



typedef struct operation operation;
typedef struct expression expression;
typedef struct unit unit;

struct unit {
  int type;
  double val;
  char* alias;
  int delta;
};

struct operation {
  int operateur;
  expression* exp1;
  expression* exp2;};

struct expression {
  int type;
  operation* op;
  unit* ele;
};


expression* Expression(char* txt, int init);

#endif  /* _FORMULA_PARSER_H */
