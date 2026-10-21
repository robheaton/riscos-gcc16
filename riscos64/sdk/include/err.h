/* err.h - the BSD err and warn functions (SDK of RISC OS 64, crt/posix/bsdcompat.c): print "program: message: strerror (errno)" on standard error */
#ifndef _ERR_H
#define _ERR_H
#include <stdarg.h>
#include <sys/cdefs.h>
#ifdef __cplusplus
extern "C" {
#endif
void warn (const char *, ...) __attribute__ ((format (printf, 1, 2)));
void vwarn (const char *, va_list) __attribute__ ((format (printf, 1, 0)));
void warnx (const char *, ...) __attribute__ ((format (printf, 1, 2)));
void vwarnx (const char *, va_list) __attribute__ ((format (printf, 1, 0)));
void warnc (int, const char *, ...) __attribute__ ((format (printf, 2, 3)));
void vwarnc (int, const char *, va_list) __attribute__ ((format (printf, 2, 0)));
void err (int, const char *, ...) __attribute__ ((noreturn, format (printf, 2, 3)));
void verr (int, const char *, va_list) __attribute__ ((noreturn, format (printf, 2, 0)));
void errx (int, const char *, ...) __attribute__ ((noreturn, format (printf, 2, 3)));
void verrx (int, const char *, va_list) __attribute__ ((noreturn, format (printf, 2, 0)));
void errc (int, int, const char *, ...) __attribute__ ((noreturn, format (printf, 3, 4)));
void verrc (int, int, const char *, va_list) __attribute__ ((noreturn, format (printf, 3, 0)));
#ifdef __cplusplus
}
#endif
#endif
