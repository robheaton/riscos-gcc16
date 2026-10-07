/* string.h - the string and memory functions of the C library (libmodkit.a).  The "C" locale only: strcoll is strcmp and strxfrm a copy; the case-insensitive comparisons fold ASCII only. */
#ifndef _STRING_H
#define _STRING_H
#include <stddef.h>
extern void *memcpy (void *d, const void *s, size_t n);
extern void *memmove (void *d, const void *s, size_t n);
extern void *memset (void *d, int c, size_t n);
extern int memcmp (const void *a, const void *b, size_t n);
extern void *memchr (const void *s, int c, size_t n);
extern size_t strlen (const char *s);
extern int strcmp (const char *a, const char *b);
extern int strncmp (const char *a, const char *b, size_t n);
extern int strcoll (const char *a, const char *b);
extern size_t strxfrm (char *d, const char *s, size_t n);
extern char *strcpy (char *d, const char *s);
extern char *strncpy (char *d, const char *s, size_t n);
extern char *strcat (char *d, const char *s);
extern char *strncat (char *d, const char *s, size_t n);
extern char *strchr (const char *s, int c);
extern char *strrchr (const char *s, int c);
extern char *strstr (const char *h, const char *n);
extern size_t strspn (const char *s, const char *accept);
extern size_t strcspn (const char *s, const char *reject);
extern char *strpbrk (const char *s, const char *accept);
extern char *strtok (char *s, const char *delim);
extern char *strerror (int errnum);
#if !defined (__STRICT_ANSI__) || defined (_GNU_SOURCE) || defined (_DEFAULT_SOURCE) || defined (_POSIX_C_SOURCE) || defined (_BSD_SOURCE)	/* (not with -std=c99 / -ansi, unless asked for: a program that defines strdup or stricmp itself has no clash) */
extern void *memccpy (void *d, const void *s, int c, size_t n);
extern size_t strnlen (const char *s, size_t n);
extern size_t strlcpy (char *d, const char *s, size_t n);
extern size_t strlcat (char *d, const char *s, size_t n);
extern char *strtok_r (char *s, const char *delim, char **save);
extern char *strsep (char **sp, const char *delim);
extern char *strdup (const char *s);
extern char *strndup (const char *s, size_t n);
extern int stricmp (const char *a, const char *b);
extern int strnicmp (const char *a, const char *b, size_t n);
extern int strcasecmp (const char *a, const char *b);
extern int strncasecmp (const char *a, const char *b, size_t n);
extern void bzero (void *p, size_t n);
extern void bcopy (const void *s, void *d, size_t n);
#endif
#endif
