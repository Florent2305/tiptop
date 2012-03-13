/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2011, 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#ifndef _VERSION_H
#define _VERSION_H

#include <config.h>

#if defined(PACKAGE_VERSION)
static const char tiptop_version[] = PACKAGE_VERSION ;
#else
static const char tiptop_version[] = "unknown" ;
#endif

#if defined(COMPILE_DATE)
static const char compile_date[] = COMPILE_DATE ;
#else
static const char compile_date[] = "unknown" ;
#endif

#if defined(COMPILE_HOST)
static const char compile_host[] = COMPILE_HOST ;
#else
static const char compile_host[] = "unknown" ;
#endif

void print_version(void);
void print_legal(void);

#endif
