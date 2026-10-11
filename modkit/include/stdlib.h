/* stdlib.h - the general utilities of the C library (libmodkit.a): memory from the RMA, numbers (strtol ... and strtod / strtof / strtold / atof, which are exact and correctly rounded: lib/strtod.c; long double is double), qsort, rand, atexit / exit / abort, getenv.  getenv reads a RISC OS
   system variable.  exit () in a module that is not "runnable" stops with an error instead (there is no program to end). */
#ifndef _STDLIB_H
#define _STDLIB_H
#ifdef __cplusplus
extern "C" {
#endif
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
extern double strtod (const char *s, char **end);
extern float strtof (const char *s, char **end);
extern long double strtold (const char *s, char **end);
extern double atof (const char *s);
extern unsigned long long strtoull (const char *s, char **end, int base);
extern int abs (int v);
extern long labs (long v);
extern long long llabs (long long v);
extern div_t div (int n, int d);
extern ldiv_t ldiv (long n, long d);
extern lldiv_t lldiv (long long n, long long d);
extern int rand (void);
#define MB_CUR_MAX 1
extern void srand (unsigned seed);
extern void qsort (void *base, size_t n, size_t size, int (*cmp) (const void *, const void *));
extern void *bsearch (const void *key, const void *base, size_t n, size_t size, int (*cmp) (const void *, const void *));
extern char *getenv (const char *name);
extern int atexit (void (*fn) (void));
extern void exit (int status) __attribute__ ((noreturn));
extern void _Exit (int status) __attribute__ ((noreturn));
extern void abort (void) __attribute__ ((noreturn));
extern int system (const char *cmd);                                         /* OS_CLI; 0, or -1 with errno EIO; system (NULL) is 1 */
extern int mblen (const char *s, size_t n);                                 /* the C locale: one byte is one character, 0 - 127; a byte above 127 is EILSEQ */
extern int mbtowc (wchar_t *pwc, const char *s, size_t n);
extern int wctomb (char *s, wchar_t wc);
extern size_t mbstowcs (wchar_t *dst, const char *src, size_t n);
extern size_t wcstombs (char *dst, const wchar_t *src, size_t n);
extern void quick_exit (int status) __attribute__ ((noreturn));            /* C11: the functions of at_quick_exit in the opposite order, then _Exit (the streams are not flushed) */
extern int at_quick_exit (void (*f) (void));
extern void *aligned_alloc (size_t alignment, size_t size);               /* C11: a power of two; the block is freed with free () */
#include <_modlib_ext.h>
#ifdef __MODLIB_EXT
extern int setenv (const char *name, const char *value, int replace);      /* system variables (OS_SetVarVal) */
extern int putenv (char *string);                                          /* "NAME=value" sets the system variable (the string is copied); a string without "=" removes it */
extern int unsetenv (const char *name);
extern char *realpath (const char *file, char *resolved);                  /* the canonical RISC OS path of an object that exists (OS_FSControl 37); resolved has FILENAME_MAX bytes, or NULL for malloc */
extern const char *getprogname (void);                                     /* the name of the program: the first word of the command line (OS_GetEnv) without its directory */
#endif
#ifdef __cplusplus
}
#endif
#endif
