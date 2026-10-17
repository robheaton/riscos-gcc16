/* pthread.c - pthread calls for a program with one thread (see include/pthread.h), and dlopen & co that always fail.  The SDK of RISC OS 64. */
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

int pthread_mutex_init (pthread_mutex_t *m, const pthread_mutexattr_t *a) { m->locked = 0; m->type = a ? a->type : 0; return 0; }
int pthread_mutex_destroy (pthread_mutex_t *m) { (void) m; return 0; }
int pthread_mutex_lock (pthread_mutex_t *m)
{
  if (m->locked && m->type != PTHREAD_MUTEX_RECURSIVE) return m->type == PTHREAD_MUTEX_ERRORCHECK ? EDEADLK : 0;	/* a normal mutex locked twice would wait for ever: go on */
  m->locked++;
  return 0;
}
int pthread_mutex_trylock (pthread_mutex_t *m)
{
  if (m->locked && m->type != PTHREAD_MUTEX_RECURSIVE) return EBUSY;
  m->locked++;
  return 0;
}
int pthread_mutex_unlock (pthread_mutex_t *m) { if (m->locked > 0) m->locked--; return 0; }
int pthread_mutexattr_init (pthread_mutexattr_t *a) { a->type = 0; return 0; }
int pthread_mutexattr_destroy (pthread_mutexattr_t *a) { (void) a; return 0; }
int pthread_mutexattr_settype (pthread_mutexattr_t *a, int t) { a->type = t; return 0; }
int pthread_cond_init (pthread_cond_t *c, const pthread_condattr_t *a) { (void) a; c->dummy = 0; return 0; }
int pthread_cond_destroy (pthread_cond_t *c) { (void) c; return 0; }
int pthread_cond_wait (pthread_cond_t *c, pthread_mutex_t *m) { (void) c; (void) m; return EDEADLK; }	/* nobody can signal */
int pthread_cond_timedwait (pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *t) { (void) c; (void) m; (void) t; return ETIMEDOUT; }
int pthread_cond_signal (pthread_cond_t *c) { (void) c; return 0; }
int pthread_cond_broadcast (pthread_cond_t *c) { (void) c; return 0; }
int pthread_once (pthread_once_t *o, void (*f) (void)) { if (!*o) { *o = 1; f (); } return 0; }

#define NKEYS 64
static struct { int used; const void *value; } keys[NKEYS];
int pthread_key_create (pthread_key_t *k, void (*d) (void *))
{
  unsigned i;
  (void) d;
  for (i = 0; i < NKEYS; i++) if (!keys[i].used) { keys[i].used = 1; keys[i].value = 0; *k = i; return 0; }
  return EAGAIN;
}
int pthread_key_delete (pthread_key_t k) { if (k >= NKEYS) return EINVAL; keys[k].used = 0; return 0; }
void *pthread_getspecific (pthread_key_t k) { return k < NKEYS && keys[k].used ? (void *) keys[k].value : 0; }
int pthread_setspecific (pthread_key_t k, const void *v) { if (k >= NKEYS || !keys[k].used) return EINVAL; keys[k].value = v; return 0; }
pthread_t pthread_self (void) { return 1; }
int pthread_equal (pthread_t a, pthread_t b) { return a == b; }
int pthread_create (pthread_t *t, const pthread_attr_t *a, void *(*f) (void *), void *arg) { (void) t; (void) a; (void) f; (void) arg; return EAGAIN; }
int pthread_join (pthread_t t, void **r) { (void) t; (void) r; return ESRCH; }
int pthread_detach (pthread_t t) { (void) t; return ESRCH; }
int pthread_attr_init (pthread_attr_t *a) { a->detach = 0; return 0; }
int pthread_attr_destroy (pthread_attr_t *a) { (void) a; return 0; }
int pthread_sigmask (int how, const sigset_t *s, sigset_t *o) { extern int sigprocmask (int, const sigset_t *, sigset_t *); return sigprocmask (how, s, o); }

void *dlopen (const char *f, int fl) { (void) f; (void) fl; return 0; }
void *dlsym (void *h, const char *s) { (void) h; (void) s; return 0; }
int dlclose (void *h) { (void) h; return -1; }
char *dlerror (void) { return (char *) "dynamic loading is not available on RISC OS 64 (programs are statically linked)"; }
