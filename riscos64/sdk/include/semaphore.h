/* semaphore.h - POSIX semaphores for the cooperative threads of pthread.h.  The SDK of RISC OS 64. */
#ifndef _RISCOS64_SEMAPHORE_H
#define _RISCOS64_SEMAPHORE_H
#include <time.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { unsigned value; unsigned dummy; } sem_t;
#define SEM_FAILED ((sem_t *) 0)
#define SEM_VALUE_MAX 2147483647
int sem_init (sem_t *, int, unsigned);
int sem_destroy (sem_t *);
int sem_wait (sem_t *);
int sem_trywait (sem_t *);
int sem_timedwait (sem_t *, const struct timespec *);
int sem_post (sem_t *);
int sem_getvalue (sem_t *, int *);
sem_t *sem_open (const char *, int, ...);
int sem_close (sem_t *);
int sem_unlink (const char *);
#ifdef __cplusplus
}
#endif
#endif
