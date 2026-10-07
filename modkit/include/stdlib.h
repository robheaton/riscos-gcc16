/* stdlib.h - the general utilities of the C library (libmodkit.a): memory from the RMA, numbers (integers only: there is no strtod), qsort, rand, atexit / exit / abort, getenv.  getenv reads a RISC OS
   system variable.  exit () in a module that is not "runnable" stops with an error instead (there is no program to end). */
#ifndef _STDLIB_H
#define _STDLIB_H
#include <stddef.h>
#ifndef NULL
#define NULL ((void *) 0)
#endif
#define EXIT_SUCCESS	0
#define EXIT_FAILURE	1
#define RAND_MAX	0x7fffffff
typedef struct { int quot, rem; } div_t;
typedef struct { long quot, rem; } ldiv_t;
typedef struct { long long quot, rem; } lldiv_t;
extern void *malloc (size_t n);
extern void *calloc (size_t n, size_t size);
extern void *realloc (void *p, size_t n);
extern void free (void *p);
extern int atoi (const char *s);
extern long atol (const char *s);
extern long long atoll (const char *s);
extern long strtol (const char *s, char **end, int base);
extern unsigned long strtoul (const char *s, char **end, int base);
extern long long strtoll (const char *s, char **end, int base);
extern unsigned long long strtoull (const char *s, char **end, int base);
extern int abs (int v);
extern long labs (long v);
extern long long llabs (long long v);
extern div_t div (int n, int d);
extern ldiv_t ldiv (long n, long d);
extern lldiv_t lldiv (long long n, long long d);
extern int rand (void);
extern void srand (unsigned seed);
extern void qsort (void *base, size_t n, size_t size, int (*cmp) (const void *, const void *));
extern void *bsearch (const void *key, const void *base, size_t n, size_t size, int (*cmp) (const void *, const void *));
extern char *getenv (const char *name);
extern int atexit (void (*fn) (void));
extern void exit (int status) __attribute__ ((noreturn));
extern void _Exit (int status) __attribute__ ((noreturn));
extern void abort (void) __attribute__ ((noreturn));
#endif
