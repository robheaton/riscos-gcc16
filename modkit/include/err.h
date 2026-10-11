/* err.h - the BSD error reporting functions: they write "program: message: error text" to stderr (the program is the first word of the command line); err, errc, errx, verr, verrc and verrx then call exit (eval). */
#ifndef _ERR_H
#define _ERR_H
#include <stdarg.h>
#ifdef __cplusplus
extern "C" {
#endif
extern void warn (const char *fmt, ...) __attribute__ ((format (__printf__, 1, 2)));
extern void warnc (int code, const char *fmt, ...) __attribute__ ((format (__printf__, 2, 3)));
extern void warnx (const char *fmt, ...) __attribute__ ((format (__printf__, 1, 2)));
extern void vwarn (const char *fmt, va_list ap);
extern void vwarnc (int code, const char *fmt, va_list ap);
extern void vwarnx (const char *fmt, va_list ap);
extern void err (int eval, const char *fmt, ...) __attribute__ ((noreturn, format (__printf__, 2, 3)));
extern void errc (int eval, int code, const char *fmt, ...) __attribute__ ((noreturn, format (__printf__, 3, 4)));
extern void errx (int eval, const char *fmt, ...) __attribute__ ((noreturn, format (__printf__, 2, 3)));
extern void verr (int eval, const char *fmt, va_list ap) __attribute__ ((noreturn));
extern void verrc (int eval, int code, const char *fmt, va_list ap) __attribute__ ((noreturn));
extern void verrx (int eval, const char *fmt, va_list ap) __attribute__ ((noreturn));
#ifdef __cplusplus
}
#endif
#endif
