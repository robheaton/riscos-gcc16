/* pthread.h - POSIX threads for a program of RISC OS 64.  RISC OS has no threads of its own, so they are made by the C library: cooperative threads in one task, each with a stack of its own, that switch when a
   thread waits (pthread_join, a locked mutex, pthread_cond_wait, sleep, sched_yield) and not otherwise.  A thread that never waits runs until it ends, and a program that waits for a flag in a loop without
   calling sched_yield () or a waiting function never lets the other threads run.  Mutexes, condition variables, once, keys, read-write locks and semaphores (<semaphore.h>) are the real ones for these threads.
   The SDK of RISC OS 64 (crt/posix/pthread.c).  */
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
typedef struct { void *owner; int count; int type; } pthread_mutex_t;
typedef struct { int type; } pthread_mutexattr_t;
typedef struct { int dummy; } pthread_cond_t;
typedef struct { int clock; } pthread_condattr_t;
typedef int pthread_once_t;
typedef unsigned pthread_key_t;
typedef struct { int detach; unsigned long stacksize; } pthread_attr_t;
typedef struct { void *writer; int readers; int dummy; } pthread_rwlock_t;
typedef struct { int dummy; } pthread_rwlockattr_t;
typedef struct { int dummy; } pthread_barrierattr_t;
typedef struct { unsigned count, waiting, cycle; } pthread_barrier_t;
typedef volatile int pthread_spinlock_t;
#define PTHREAD_MUTEX_INITIALIZER { 0, 0, 0 }
#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP { 0, 0, 1 }
#define PTHREAD_COND_INITIALIZER { 0 }
#define PTHREAD_RWLOCK_INITIALIZER { 0, 0, 0 }
#define PTHREAD_ONCE_INIT 0
#define PTHREAD_MUTEX_NORMAL 0
#define PTHREAD_MUTEX_RECURSIVE 1
#define PTHREAD_MUTEX_ERRORCHECK 2
#define PTHREAD_MUTEX_DEFAULT 0
#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1
#define PTHREAD_CANCEL_ENABLE 0
#define PTHREAD_CANCEL_DISABLE 1
#define PTHREAD_CANCEL_DEFERRED 0
#define PTHREAD_CANCEL_ASYNCHRONOUS 1
#define PTHREAD_BARRIER_SERIAL_THREAD (-1)
#define PTHREAD_STACK_MIN 16384

int pthread_create (pthread_t *, const pthread_attr_t *, void *(*) (void *), void *);
int pthread_join (pthread_t, void **);
int pthread_detach (pthread_t);
void pthread_exit (void *) __attribute__ ((__noreturn__));
pthread_t pthread_self (void);
int pthread_equal (pthread_t, pthread_t);
int pthread_yield (void);
int sched_yield (void);				/* (picolibc declares it only for some feature macros) */
int pthread_cancel (pthread_t);
int pthread_kill (pthread_t, int);
int pthread_setcancelstate (int, int *);
int pthread_setcanceltype (int, int *);
void pthread_testcancel (void);
int pthread_setname_np (pthread_t, const char *);
int pthread_getname_np (pthread_t, char *, size_t);

int pthread_attr_init (pthread_attr_t *);
int pthread_attr_destroy (pthread_attr_t *);
int pthread_attr_setdetachstate (pthread_attr_t *, int);
int pthread_attr_getdetachstate (const pthread_attr_t *, int *);
int pthread_attr_setstacksize (pthread_attr_t *, size_t);
int pthread_attr_getstacksize (const pthread_attr_t *, size_t *);
int pthread_getattr_np (pthread_t, pthread_attr_t *);

int pthread_mutex_init (pthread_mutex_t *, const pthread_mutexattr_t *);
int pthread_mutex_destroy (pthread_mutex_t *);
int pthread_mutex_lock (pthread_mutex_t *);
int pthread_mutex_trylock (pthread_mutex_t *);
int pthread_mutex_timedlock (pthread_mutex_t *, const struct timespec *);
int pthread_mutex_unlock (pthread_mutex_t *);
int pthread_mutexattr_init (pthread_mutexattr_t *);
int pthread_mutexattr_destroy (pthread_mutexattr_t *);
int pthread_mutexattr_settype (pthread_mutexattr_t *, int);
int pthread_mutexattr_gettype (const pthread_mutexattr_t *, int *);

int pthread_cond_init (pthread_cond_t *, const pthread_condattr_t *);
int pthread_cond_destroy (pthread_cond_t *);
int pthread_cond_wait (pthread_cond_t *, pthread_mutex_t *);
int pthread_cond_timedwait (pthread_cond_t *, pthread_mutex_t *, const struct timespec *);
int pthread_cond_clockwait (pthread_cond_t *, pthread_mutex_t *, clockid_t, const struct timespec *);
int pthread_cond_signal (pthread_cond_t *);
int pthread_cond_broadcast (pthread_cond_t *);
int pthread_condattr_init (pthread_condattr_t *);
int pthread_condattr_destroy (pthread_condattr_t *);
int pthread_condattr_setclock (pthread_condattr_t *, clockid_t);
int pthread_condattr_getclock (const pthread_condattr_t *, clockid_t *);

int pthread_rwlock_init (pthread_rwlock_t *, const pthread_rwlockattr_t *);
int pthread_rwlock_destroy (pthread_rwlock_t *);
int pthread_rwlock_rdlock (pthread_rwlock_t *);
int pthread_rwlock_tryrdlock (pthread_rwlock_t *);
int pthread_rwlock_wrlock (pthread_rwlock_t *);
int pthread_rwlock_trywrlock (pthread_rwlock_t *);
int pthread_rwlock_unlock (pthread_rwlock_t *);

int pthread_barrier_init (pthread_barrier_t *, const pthread_barrierattr_t *, unsigned);
int pthread_barrier_destroy (pthread_barrier_t *);
int pthread_barrier_wait (pthread_barrier_t *);
int pthread_spin_init (pthread_spinlock_t *, int);
int pthread_spin_destroy (pthread_spinlock_t *);
int pthread_spin_lock (pthread_spinlock_t *);
int pthread_spin_trylock (pthread_spinlock_t *);
int pthread_spin_unlock (pthread_spinlock_t *);

int pthread_once (pthread_once_t *, void (*) (void));
int pthread_key_create (pthread_key_t *, void (*) (void *));
int pthread_key_delete (pthread_key_t);
void *pthread_getspecific (pthread_key_t);
int pthread_setspecific (pthread_key_t, const void *);
int pthread_sigmask (int, const sigset_t *, sigset_t *);
int pthread_atfork (void (*) (void), void (*) (void), void (*) (void));
#ifdef __cplusplus
}
#endif
#endif
