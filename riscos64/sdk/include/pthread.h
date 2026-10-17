/* pthread.h - the POSIX thread calls for a program of RISC OS 64 that has one thread: mutexes, condition variables, once and keys work as in a single-threaded program (a lock is a flag, waiting on a
   condition that nobody can signal returns an error), pthread_create fails with EAGAIN.  Enough for programs (GCC, binutils, libstdc++'s gthr) that link the calls but never start a thread.  The SDK of RISC OS 64. */
#ifndef _RISCOS64_PTHREAD_H
#define _RISCOS64_PTHREAD_H
#include <sys/types.h>
#include <time.h>
#include <sched.h>
#include <signal.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef unsigned long pthread_t;
typedef struct { int locked; int type; } pthread_mutex_t;
typedef struct { int type; } pthread_mutexattr_t;
typedef struct { int dummy; } pthread_cond_t;
typedef struct { int dummy; } pthread_condattr_t;
typedef int pthread_once_t;
typedef unsigned pthread_key_t;
typedef struct { int detach; } pthread_attr_t;
#define PTHREAD_MUTEX_INITIALIZER { 0, 0 }
#define PTHREAD_COND_INITIALIZER { 0 }
#define PTHREAD_ONCE_INIT 0
#define PTHREAD_MUTEX_NORMAL 0
#define PTHREAD_MUTEX_RECURSIVE 1
#define PTHREAD_MUTEX_ERRORCHECK 2
#define PTHREAD_MUTEX_DEFAULT 0
int pthread_mutex_init (pthread_mutex_t *, const pthread_mutexattr_t *);
int pthread_mutex_destroy (pthread_mutex_t *);
int pthread_mutex_lock (pthread_mutex_t *);
int pthread_mutex_trylock (pthread_mutex_t *);
int pthread_mutex_unlock (pthread_mutex_t *);
int pthread_mutexattr_init (pthread_mutexattr_t *);
int pthread_mutexattr_destroy (pthread_mutexattr_t *);
int pthread_mutexattr_settype (pthread_mutexattr_t *, int);
int pthread_cond_init (pthread_cond_t *, const pthread_condattr_t *);
int pthread_cond_destroy (pthread_cond_t *);
int pthread_cond_wait (pthread_cond_t *, pthread_mutex_t *);
int pthread_cond_timedwait (pthread_cond_t *, pthread_mutex_t *, const struct timespec *);
int pthread_cond_signal (pthread_cond_t *);
int pthread_cond_broadcast (pthread_cond_t *);
int pthread_once (pthread_once_t *, void (*) (void));
int pthread_key_create (pthread_key_t *, void (*) (void *));
int pthread_key_delete (pthread_key_t);
void *pthread_getspecific (pthread_key_t);
int pthread_setspecific (pthread_key_t, const void *);
pthread_t pthread_self (void);
int pthread_equal (pthread_t, pthread_t);
int pthread_create (pthread_t *, const pthread_attr_t *, void *(*) (void *), void *);
int pthread_join (pthread_t, void **);
int pthread_detach (pthread_t);
int pthread_attr_init (pthread_attr_t *);
int pthread_attr_destroy (pthread_attr_t *);
int pthread_sigmask (int, const sigset_t *, sigset_t *);
#ifdef __cplusplus
}
#endif
#endif
