/*
 * This file is part of tiptop.
 *
 * Author: Antoine NAUDIN
 * Copyright (c) 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#ifndef _WRITE_CONFIG_H
#define _WRITE_CONFIG_H

#include <stdio.h>

#include "screen.h"
#include "options.h"


int build_configuration(screen_t** sc, int num_screens, struct option* o,
                        FILE* file);

#endif  /* _WRITE_CONFIG_H */
