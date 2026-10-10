/* signal.h - signal () and raise () for the C library of modkit: the handlers are kept in a table and raise () calls them (ISO C); nothing in a module generates a signal by itself, so these are only
   useful for code that raises its own.  The signal numbers are the SharedCLibrary's.  The default action of raise () is abort () (a module has no program to end: it stops with an error). */
#ifndef _SIGNAL_H
#define _SIGNAL_H
#ifdef __cplusplus
extern "C" {
#endif
typedef int sig_atomic_t;
typedef void (*__sighandler_t) (int);
#define SIG_DFL	((__sighandler_t) 0)
#define SIG_IGN	((__sighandler_t) 1)
#define SIG_ERR	((__sighandler_t) -1)
#define SIGABRT	1
#define SIGFPE	2
#define SIGILL	3
#define SIGINT	4
#define SIGSEGV	5
#define SIGTERM	6
#define SIGSTAK	7
#define SIGUSR1	8
#define SIGUSR2	9
#define SIGOSERROR	10
#define _NSIG	11
extern __sighandler_t signal (int sig, __sighandler_t func);
extern int raise (int sig);
#ifdef __cplusplus
}
#endif
#endif
