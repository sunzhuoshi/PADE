//
//  common.h
//  PADE
//
//  Created by sun zhuoshi on 4/15/13.
//  Copyright (c) 2013 l4play. All rights reserved.
//

#ifndef PADE_common_h
#define PADE_common_h

#include <stdio.h>
#include <stdarg.h>

#define FCC_TEX1 "TEX1"

void abort_(const char * s, ...)
{
    va_list args;
    va_start(args, s);
    vfprintf(stderr, s, args);
    fprintf(stderr, "\n");
    va_end(args);
    abort();
}

#endif
