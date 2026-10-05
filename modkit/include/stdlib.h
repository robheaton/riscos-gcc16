#ifndef _STDLIB_H
#define _STDLIB_H
#include <stddef.h>
#define NULL ((void *) 0)
extern void *malloc (size_t n);
extern void *calloc (size_t n, size_t size);
extern void *realloc (void *p, size_t n);
extern void free (void *p);
extern int atoi (const char *s);
extern long strtol (const char *s, char **end, int base);
extern unsigned long strtoul (const char *s, char **end, int base);
extern int abs (int v);
#endif
