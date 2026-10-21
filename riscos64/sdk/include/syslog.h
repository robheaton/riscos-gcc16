/* syslog.h - nothing is kept: the calls do nothing.  The SDK of RISC OS 64. */
#ifndef _SYSLOG_H
#define _SYSLOG_H
#define LOG_EMERG 0
#define LOG_ALERT 1
#define LOG_CRIT 2
#define LOG_ERR 3
#define LOG_WARNING 4
#define LOG_NOTICE 5
#define LOG_INFO 6
#define LOG_DEBUG 7
#define LOG_PID 1
#define LOG_CONS 2
#define LOG_NDELAY 8
#define LOG_USER (1 << 3)
#define LOG_DAEMON (3 << 3)
#define LOG_LOCAL0 (16 << 3)
#define LOG_MASK(p) (1 << (p))
#define LOG_UPTO(p) ((1 << ((p) + 1)) - 1)
#include <stdarg.h>
#define LOG_PERROR 0x20
#define LOG_AUTH (4 << 3)
#define LOG_MAIL (2 << 3)
#define LOG_LOCAL1 (17 << 3)
#ifdef __cplusplus
extern "C" {
#endif
void openlog (const char *, int, int);
void closelog (void);
void syslog (int, const char *, ...);
void vsyslog (int, const char *, va_list);
int setlogmask (int);
#ifdef __cplusplus
}
#endif
#endif
