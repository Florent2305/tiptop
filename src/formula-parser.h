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


#define CONST   0
#define COUNT   1

#define OPER    10
#define ELEM    11

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
  expression* exp2;
};

struct expression {
  int type;
  operation* op;
  unit* ele;
};


expression* Expression(char* txt, int init);

#endif  /* _FORMULA_PARSER_H */
