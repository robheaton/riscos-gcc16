/* stdio.h - printf writes to the screen with OS_WriteC (there are no files yet); sprintf / snprintf / vsnprintf format into memory; sscanf / vsscanf read integers, characters and strings from memory (no
   floating point conversions). */
#ifndef _STDIO_H
#define _STDIO_H
#include <stddef.h>
#include <stdarg.h>
#ifndef EOF
#define EOF (-1)
#endif
extern int printf (const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
extern int sprintf (char *s, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
extern int snprintf (char *s, size_t n, const char *fmt, ...) __attribute__ ((format (printf, 3, 4)));
extern int vsnprintf (char *s, size_t n, const char *fmt, va_list ap);
extern int vsprintf (char *s, const char *fmt, va_list ap);
extern int vprintf (const char *fmt, va_list ap);
extern int sscanf (const char *s, const char *fmt, ...) __attribute__ ((format (scanf, 2, 3)));
extern int vsscanf (const char *s, const char *fmt, va_list ap);
extern int puts (const char *s);
extern int putchar (int c);
#endif
