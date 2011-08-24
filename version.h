#ifndef _VERSION_H
#define _VERSION_H

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
