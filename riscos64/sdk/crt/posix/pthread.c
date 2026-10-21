/* pthread.c - POSIX threads for RISC OS 64, and POSIX semaphores: cooperative threads in one task (see include/pthread.h).  Every thread has a stack of its own (malloc'd) and a thread-local block (the .tdata and .tbss of
   the program, copied; tpidr_el0 points at it).  A switch happens only inside this file, when the running thread has to wait: pthread_join, a locked mutex, pthread_cond_wait, a read-write lock, a semaphore, a
   sleep (timefn.c calls __ul64_thread_yield), sched_yield, or the end of the thread.  Then the next thread that can run is chosen, round robin; when none can, the waiters with a timeout are polled, and when there
   are none the program is dead locked, which is reported and ends it.  Mutexes pass to the oldest waiter when they are unlocked.  Without a second thread nothing here costs more than a few instructions.
   The SDK of RISC OS 64.  */
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);
extern void __ul64_poll_signals (void);
extern void _exit (int);
extern char __tdata_start[], __tdata_end[], __tbss_start[], __tbss_end[], __tls_align[];

#define STACK_DEFAULT (256 * 1024)
#define NKEYS 128
#define DESTRUCTOR_ITERATIONS 4

enum { T_RUN, T_BLOCK, T_ZOMBIE };

typedef struct thr
{
  struct thr *next;				/* all threads, in the order they were made */
  int state;
  int detached;
  int reapable;					/* a zombie nobody will join: free it when no thread runs on its stack */
  unsigned long ctx[22];			/* x19-x30 (0-11), sp (12), d8-d15 (13-20), tpidr_el0 (21) */
  char *stack;
  size_t stack_size;
  char *tls_mem;
  unsigned long tp;
  void *(*start) (void *);
  void *arg;
  void *retval;
  const void *obj;				/* what a blocked thread waits for */
  int result;					/* 0, or ETIMEDOUT when the wait timed out */
  int timed;
  unsigned long deadline;			/* centiseconds on the monotonic clock */
  unsigned long seq;				/* the order of waiting: the oldest waiter gets a mutex first */
  char name[16];
  const void *keys[NKEYS];
} thr;

static thr main_thr = { .state = T_RUN };
static thr *head = &main_thr, *tail = &main_thr, *cur = &main_thr;
int __ul64_nthreads = 1;			/* threads that have not ended (zombies are not counted) */
extern int __pthread_num_running_threads __attribute__ ((alias ("__ul64_nthreads")));	/* the name UnixLib uses (its tests read it) */
static unsigned long wait_seq;
static unsigned char key_used[NKEYS];
static void (*key_destructor[NKEYS]) (void *);

__asm__ (
  ".text\n.balign 16\n.global ul64_ctxswap\n.type ul64_ctxswap, %function\n"
  "ul64_ctxswap:\n"				/* x0 = where to save the running thread, x1 = the context to run */
  "	stp	x19, x20, [x0]\n	stp	x21, x22, [x0, #16]\n	stp	x23, x24, [x0, #32]\n	stp	x25, x26, [x0, #48]\n"
  "	stp	x27, x28, [x0, #64]\n	stp	x29, x30, [x0, #80]\n"
  "	mov	x2, sp\n	str	x2, [x0, #96]\n"
  "	stp	d8, d9, [x0, #104]\n	stp	d10, d11, [x0, #120]\n	stp	d12, d13, [x0, #136]\n	stp	d14, d15, [x0, #152]\n"
  "	mrs	x2, tpidr_el0\n	str	x2, [x0, #168]\n"
  "	ldp	x19, x20, [x1]\n	ldp	x21, x22, [x1, #16]\n	ldp	x23, x24, [x1, #32]\n	ldp	x25, x26, [x1, #48]\n"
  "	ldp	x27, x28, [x1, #64]\n	ldp	x29, x30, [x1, #80]\n"
  "	ldr	x2, [x1, #96]\n	mov	sp, x2\n"
  "	ldp	d8, d9, [x1, #104]\n	ldp	d10, d11, [x1, #120]\n	ldp	d12, d13, [x1, #136]\n	ldp	d14, d15, [x1, #152]\n"
  "	ldr	x2, [x1, #168]\n	msr	tpidr_el0, x2\n"
  "	ret\n"
  ".size ul64_ctxswap, .-ul64_ctxswap\n");
extern void ul64_ctxswap (unsigned long *save, const unsigned long *load);

static unsigned long now_cs (void)
{
  unsigned long r[10] = { 0 };
  swi_x (0x42, r);					/* OS_ReadMonotonicTime */
  return r[0] & 0xFFFFFFFFUL;
}
static int reached (unsigned long deadline) { return ((now_cs () - deadline) & 0xFFFFFFFFUL) < 0x80000000UL; }

static void say (const char *s) { write (2, s, strlen (s)); }

/* free the zombies that nobody will join, except the one we are running on */
static void reap (void)
{
  thr **pp = &head->next, *prev = head, *t;
  (void) pp;
  for (t = head->next; t;)
    {
      thr *next = t->next;
      if (t->state == T_ZOMBIE && t->reapable && t != cur)
	{
	  prev->next = next;
	  if (tail == t) tail = prev;
	  free (t->stack); free (t->tls_mem); free (t);
	}
      else
	prev = t;
      t = next;
    }
}

/* choose the next thread and switch to it; the caller has set its own state (RUN to yield, BLOCK or ZOMBIE) */
static void schedule (void)
{
  for (;;)
    {
      thr *t, *pick = 0;
      int timed = 0;
      for (t = head; t; t = t->next)
	if (t->state == T_BLOCK && t->timed)
	  {
	    if (reached (t->deadline)) { t->state = T_RUN; t->timed = 0; t->obj = 0; t->result = ETIMEDOUT; }
	    else timed = 1;
	  }
      for (t = cur->next ? cur->next : head;; t = t->next ? t->next : head)
	{
	  if (t->state == T_RUN) { pick = t; break; }
	  if (t == cur) break;
	}
      if (pick)
	{
	  if (pick != cur)
	    {
	      thr *prev = cur;
	      cur = pick;
	      ul64_ctxswap (prev->ctx, pick->ctx);
	      reap ();
	    }
	  return;
	}
      if (timed) { __ul64_poll_signals (); continue; }	/* everybody waits, somebody with a time limit: wait for it */
      say ("pthread: dead lock: every thread is waiting for another\n");
      _exit (1);
    }
}

/* the current thread waits for OBJ (until DEADLINE if TIMED); returns 0 when woken, ETIMEDOUT when the time ran out */
static int block_on (const void *obj, int timed, unsigned long deadline)
{
  cur->state = T_BLOCK; cur->obj = obj; cur->timed = timed; cur->deadline = deadline; cur->result = 0; cur->seq = ++wait_seq;
  schedule ();
  return cur->result;
}
static thr *oldest_waiter (const void *obj)
{
  thr *t, *best = 0;
  for (t = head; t; t = t->next)
    if (t->state == T_BLOCK && t->obj == obj && (!best || t->seq < best->seq)) best = t;
  return best;
}
static void wake (thr *t) { t->state = T_RUN; t->obj = 0; t->timed = 0; t->result = 0; }
static int wake_one (const void *obj) { thr *t = oldest_waiter (obj); if (!t) return 0; wake (t); return 1; }
static void wake_all (const void *obj) { thr *t; for (t = head; t; t = t->next) if (t->state == T_BLOCK && t->obj == obj) wake (t); }

/* other threads get their turn (sleeps, sched_yield, spin waits) */
void __ul64_thread_yield (void)
{
  if (__ul64_nthreads > 1) { cur->state = T_RUN; schedule (); }
}
/* Threads are switched only at calls that block.  A thread that waits by polling the clock (a busy loop on clock (), times (), gettimeofday () or clock_gettime ())
   would never let the others run, and the signals of a condition variable that it sends meanwhile would be lost (UnixLib's pthread/cond test).  So the calls that read
   the time give the others a turn, at most once for each centisecond of the clock (a loop that polls a million times yields about a hundred times a second). */
void __ul64_timeslice (void)
{
  static unsigned long last;
  unsigned long r[10] = { 0 };
  if (__ul64_nthreads <= 1) return;
  swi_x (0x42, r);						/* OS_ReadMonotonicTime */
  if ((r[0] & 0xFFFFFFFFUL) == last) return;
  last = r[0] & 0xFFFFFFFFUL;
  __ul64_thread_yield ();
}
int sched_yield (void) { __ul64_thread_yield (); return 0; }
int pthread_yield (void) { __ul64_thread_yield (); return 0; }

/* The lowest address of the stack of the main thread that is in use (for the save and restore of a program that runs a child, runswap.c): that of the running thread if it is the main one, else the stack pointer
   that the main thread was suspended with.  The other threads' stacks are in the heap, which a child does not touch.  */
unsigned long __ul64_main_stack_low (unsigned long cur_sp)
{
  if (cur == &main_thr || main_thr.ctx[12] == 0) return cur_sp;
  return main_thr.ctx[12] - 256;
}

/* ---- threads ---- */

static void run_destructors (void)
{
  int round, i, again = 1;
  for (round = 0; again && round < DESTRUCTOR_ITERATIONS; round++)
    {
      again = 0;
      for (i = 0; i < NKEYS; i++)
	if (key_used[i] && cur->keys[i] && key_destructor[i])
	  {
	    void *v = (void *) cur->keys[i];
	    cur->keys[i] = 0;
	    key_destructor[i] (v);
	    again = 1;
	  }
    }
}

void pthread_exit (void *rv)
{
  run_destructors ();
  cur->retval = rv;
  if (cur == &main_thr)
    {						/* the main thread ends: the program ends when the others have */
      for (;;)
	{
	  if (__ul64_nthreads <= 1) exit (0);
	  __ul64_thread_yield ();
	  if (__ul64_nthreads > 1) { thr *t; int running = 0; for (t = head; t; t = t->next) if (t != cur && t->state != T_ZOMBIE) running = 1; if (!running) exit (0); }
	}
    }
  cur->state = T_ZOMBIE;
  cur->reapable = cur->detached;
  __ul64_nthreads--;
  wake_all (cur);
  schedule ();
  for (;;) ;					/* (never resumed) */
}

static void thread_entry (void)
{
  void *r = cur->start (cur->arg);
  pthread_exit (r);
}

int pthread_create (pthread_t *id, const pthread_attr_t *a, void *(*fn) (void *), void *arg)
{
  thr *t = (thr *) calloc (1, sizeof *t);
  size_t ssize = a && a->stacksize ? (size_t) a->stacksize : STACK_DEFAULT;
  unsigned long al = (unsigned long) __tls_align, tcb, total, block;
  if (!t) return EAGAIN;
  if (ssize < PTHREAD_STACK_MIN) ssize = PTHREAD_STACK_MIN;
  t->stack = (char *) malloc (ssize + 16);
  if (!t->stack) { free (t); return EAGAIN; }
  t->stack_size = ssize;
  if (al < 16) al = 16;
  tcb = (16 + al - 1) & ~(al - 1);
  total = (unsigned long) (__tbss_end - __tdata_start);
  t->tls_mem = (char *) malloc (total + al);
  if (!t->tls_mem) { free (t->stack); free (t); return EAGAIN; }
  block = ((unsigned long) t->tls_mem + al - 1) & ~(al - 1);
  memcpy ((void *) block, __tdata_start, (size_t) (__tdata_end - __tdata_start));
  memset ((void *) (block + (unsigned long) (__tbss_start - __tdata_start)), 0, (size_t) (__tbss_end - __tbss_start));
  t->tp = block - tcb;
  t->start = fn; t->arg = arg; t->detached = a ? a->detach : 0; t->state = T_RUN;
  t->ctx[11] = (unsigned long) thread_entry;				/* x30: where the first switch "returns" to */
  t->ctx[12] = ((unsigned long) t->stack + ssize) & ~15UL;		/* sp */
  t->ctx[21] = t->tp;
  tail->next = t; tail = t;
  __ul64_nthreads++;
  *id = (pthread_t) t;
  return 0;
}

static thr *find (pthread_t id) { thr *t; for (t = head; t; t = t->next) if ((pthread_t) t == id) return t; return 0; }

int pthread_join (pthread_t id, void **rv)
{
  thr *t = find (id);
  if (!t) return ESRCH;
  if (t == cur) return EDEADLK;
  if (t->detached) return EINVAL;
  while (t->state != T_ZOMBIE) block_on (t, 0, 0);
  if (rv) *rv = t->retval;
  t->detached = 1; t->reapable = 1;
  reap ();
  return 0;
}
int pthread_detach (pthread_t id)
{
  thr *t = find (id);
  if (!t) return ESRCH;
  t->detached = 1;
  if (t->state == T_ZOMBIE) { t->reapable = 1; reap (); }
  return 0;
}
pthread_t pthread_self (void) { return (pthread_t) cur; }
int pthread_equal (pthread_t a, pthread_t b) { return a == b; }
int pthread_cancel (pthread_t t) { (void) t; return ENOSYS; }
int pthread_kill (pthread_t t, int sig) { thr *x = find (t); if (!x) return ESRCH; if (sig == 0) return 0; return x == cur ? raise (sig) : ENOSYS; }
int pthread_setcancelstate (int s, int *old) { if (old) *old = PTHREAD_CANCEL_DISABLE; (void) s; return 0; }
int pthread_setcanceltype (int s, int *old) { if (old) *old = PTHREAD_CANCEL_DEFERRED; (void) s; return 0; }
void pthread_testcancel (void) {}
int pthread_setname_np (pthread_t t, const char *n) { thr *x = find (t); if (!x) return ESRCH; strncpy (x->name, n, sizeof x->name - 1); return 0; }
int pthread_getname_np (pthread_t t, char *n, size_t len) { thr *x = find (t); if (!x) return ESRCH; strncpy (n, x->name, len); if (len) n[len - 1] = 0; return 0; }
int pthread_atfork (void (*a) (void), void (*b) (void), void (*c) (void)) { (void) a; (void) b; (void) c; return 0; }
int pthread_sigmask (int how, const sigset_t *s, sigset_t *o) { extern int sigprocmask (int, const sigset_t *, sigset_t *); return sigprocmask (how, s, o); }

int pthread_attr_init (pthread_attr_t *a) { a->detach = 0; a->stacksize = 0; return 0; }
int pthread_attr_destroy (pthread_attr_t *a) { (void) a; return 0; }
int pthread_attr_setdetachstate (pthread_attr_t *a, int d) { a->detach = d; return 0; }
int pthread_attr_getdetachstate (const pthread_attr_t *a, int *d) { *d = a->detach; return 0; }
int pthread_attr_setstacksize (pthread_attr_t *a, size_t s) { if (s < PTHREAD_STACK_MIN) return EINVAL; a->stacksize = s; return 0; }
int pthread_attr_getstacksize (const pthread_attr_t *a, size_t *s) { *s = a->stacksize ? (size_t) a->stacksize : STACK_DEFAULT; return 0; }
int pthread_getattr_np (pthread_t t, pthread_attr_t *a) { thr *x = find (t); if (!x) return ESRCH; a->detach = x->detached; a->stacksize = x->stack_size ? x->stack_size : STACK_DEFAULT; return 0; }

/* ---- mutexes ---- */

int pthread_mutex_init (pthread_mutex_t *m, const pthread_mutexattr_t *a) { m->owner = 0; m->count = 0; m->type = a ? a->type : 0; return 0; }
int pthread_mutex_destroy (pthread_mutex_t *m) { (void) m; return 0; }
int pthread_mutex_trylock (pthread_mutex_t *m)
{
  if (!m->owner) { m->owner = cur; m->count = 1; return 0; }
  if (m->owner == cur && m->type == PTHREAD_MUTEX_RECURSIVE) { m->count++; return 0; }
  return EBUSY;
}
static int mutex_lock (pthread_mutex_t *m, int timed, unsigned long deadline)
{
  if (!m->owner) { m->owner = cur; m->count = 1; return 0; }
  if (m->owner == cur)
    {
      if (m->type == PTHREAD_MUTEX_RECURSIVE) { m->count++; return 0; }
      if (m->type == PTHREAD_MUTEX_ERRORCHECK) return EDEADLK;
    }
  /* wait: unlock hands the mutex to the oldest waiter, so owner == cur when this returns 0 */
  if (block_on (m, timed, deadline) == ETIMEDOUT) return ETIMEDOUT;
  return 0;
}
int pthread_mutex_lock (pthread_mutex_t *m) { return mutex_lock (m, 0, 0); }
int pthread_mutex_timedlock (pthread_mutex_t *m, const struct timespec *abs)
{
  struct timespec now;
  long long d;
  clock_gettime (CLOCK_REALTIME, &now);
  d = ((long long) abs->tv_sec - now.tv_sec) * 1000000000LL + (abs->tv_nsec - now.tv_nsec);
  if (d < 0) d = 0;
  return mutex_lock (m, 1, now_cs () + (unsigned long) ((d + 9999999LL) / 10000000LL));
}
int pthread_mutex_unlock (pthread_mutex_t *m)
{
  thr *w;
  if (m->owner != cur && m->owner) { if (m->type != PTHREAD_MUTEX_NORMAL) return EPERM; }
  if (m->owner == cur && m->count > 1) { m->count--; return 0; }
  w = oldest_waiter (m);
  if (w) { m->owner = w; m->count = 1; wake (w); }
  else { m->owner = 0; m->count = 0; }
  return 0;
}
int pthread_mutexattr_init (pthread_mutexattr_t *a) { a->type = 0; return 0; }
int pthread_mutexattr_destroy (pthread_mutexattr_t *a) { (void) a; return 0; }
int pthread_mutexattr_settype (pthread_mutexattr_t *a, int t) { a->type = t; return 0; }
int pthread_mutexattr_gettype (const pthread_mutexattr_t *a, int *t) { *t = a->type; return 0; }

/* ---- condition variables ---- */

int pthread_cond_init (pthread_cond_t *c, const pthread_condattr_t *a) { (void) a; c->dummy = 0; return 0; }
int pthread_cond_destroy (pthread_cond_t *c) { (void) c; return 0; }
static int cond_wait (pthread_cond_t *c, pthread_mutex_t *m, int timed, unsigned long deadline)
{
  int saved = m->count, r;
  m->count = 1;
  pthread_mutex_unlock (m);
  r = block_on (c, timed, deadline);
  pthread_mutex_lock (m);
  m->count = saved ? saved : 1;
  return r;
}
int pthread_cond_wait (pthread_cond_t *c, pthread_mutex_t *m) { return cond_wait (c, m, 0, 0); }
int pthread_cond_clockwait (pthread_cond_t *c, pthread_mutex_t *m, clockid_t clk, const struct timespec *abs)
{
  struct timespec now;
  long long d;
  clock_gettime (clk, &now);
  d = ((long long) abs->tv_sec - now.tv_sec) * 1000000000LL + (abs->tv_nsec - now.tv_nsec);
  if (d < 0) d = 0;
  return cond_wait (c, m, 1, now_cs () + (unsigned long) ((d + 9999999LL) / 10000000LL));
}
int pthread_cond_timedwait (pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *abs) { return pthread_cond_clockwait (c, m, CLOCK_REALTIME, abs); }
int pthread_cond_signal (pthread_cond_t *c) { wake_one (c); return 0; }
int pthread_cond_broadcast (pthread_cond_t *c) { wake_all (c); return 0; }
int pthread_condattr_init (pthread_condattr_t *a) { a->clock = CLOCK_REALTIME; return 0; }
int pthread_condattr_destroy (pthread_condattr_t *a) { (void) a; return 0; }
int pthread_condattr_setclock (pthread_condattr_t *a, clockid_t c) { a->clock = (int) c; return 0; }
int pthread_condattr_getclock (const pthread_condattr_t *a, clockid_t *c) { *c = (clockid_t) a->clock; return 0; }

/* ---- read-write locks, barriers, spin locks ---- */

int pthread_rwlock_init (pthread_rwlock_t *l, const pthread_rwlockattr_t *a) { (void) a; l->writer = 0; l->readers = 0; return 0; }
int pthread_rwlock_destroy (pthread_rwlock_t *l) { (void) l; return 0; }
int pthread_rwlock_tryrdlock (pthread_rwlock_t *l) { if (l->writer) return EBUSY; l->readers++; return 0; }
int pthread_rwlock_trywrlock (pthread_rwlock_t *l) { if (l->writer || l->readers) return EBUSY; l->writer = cur; return 0; }
int pthread_rwlock_rdlock (pthread_rwlock_t *l) { while (l->writer) block_on (l, 0, 0); l->readers++; return 0; }
int pthread_rwlock_wrlock (pthread_rwlock_t *l) { while (l->writer || l->readers) block_on (l, 0, 0); l->writer = cur; return 0; }
int pthread_rwlock_unlock (pthread_rwlock_t *l)
{
  if (l->writer == cur) l->writer = 0; else if (l->readers > 0) l->readers--; else return EPERM;
  wake_all (l);
  return 0;
}
int pthread_barrier_init (pthread_barrier_t *b, const pthread_barrierattr_t *a, unsigned n) { (void) a; if (!n) return EINVAL; b->count = n; b->waiting = 0; b->cycle = 0; return 0; }
int pthread_barrier_destroy (pthread_barrier_t *b) { (void) b; return 0; }
int pthread_barrier_wait (pthread_barrier_t *b)
{
  unsigned cycle = b->cycle;
  if (++b->waiting == b->count) { b->waiting = 0; b->cycle++; wake_all (b); return PTHREAD_BARRIER_SERIAL_THREAD; }
  while (b->cycle == cycle) block_on (b, 0, 0);
  return 0;
}
int pthread_spin_init (pthread_spinlock_t *s, int shared) { (void) shared; *s = 0; return 0; }
int pthread_spin_destroy (pthread_spinlock_t *s) { (void) s; return 0; }
int pthread_spin_lock (pthread_spinlock_t *s) { while (*s) __ul64_thread_yield (); *s = 1; return 0; }
int pthread_spin_trylock (pthread_spinlock_t *s) { if (*s) return EBUSY; *s = 1; return 0; }
int pthread_spin_unlock (pthread_spinlock_t *s) { *s = 0; return 0; }

/* ---- once and keys ---- */

static void once_failed (pthread_once_t **g) { if (*g) { **g = 0; wake_all (*g); } }

int pthread_once (pthread_once_t *o, void (*f) (void))
{
  if (*o == 2) return 0;
  if (*o == 1) { while (*o == 1) block_on (o, 1, now_cs () + 1); return 0; }	/* somebody is running it (and has yielded): wait */
  *o = 1;
  { pthread_once_t *guard __attribute__ ((cleanup (once_failed))) = o;	/* f () throws (C++): the next caller tries again */
    f ();
    guard = 0; }
  *o = 2;
  wake_all (o);
  return 0;
}
int pthread_key_create (pthread_key_t *k, void (*d) (void *))
{
  unsigned i;
  for (i = 0; i < NKEYS; i++) if (!key_used[i]) { key_used[i] = 1; key_destructor[i] = d; *k = i; return 0; }
  return EAGAIN;
}
int pthread_key_delete (pthread_key_t k) { thr *t; if (k >= NKEYS || !key_used[k]) return EINVAL; key_used[k] = 0; for (t = head; t; t = t->next) t->keys[k] = 0; return 0; }
void *pthread_getspecific (pthread_key_t k) { return k < NKEYS && key_used[k] ? (void *) cur->keys[k] : 0; }
int pthread_setspecific (pthread_key_t k, const void *v) { if (k >= NKEYS || !key_used[k]) return EINVAL; cur->keys[k] = v; return 0; }

/* ---- semaphores ---- */

int sem_init (sem_t *s, int shared, unsigned v) { (void) shared; s->value = v; return 0; }
int sem_destroy (sem_t *s) { (void) s; return 0; }
int sem_post (sem_t *s) { s->value++; wake_one (s); return 0; }
int sem_trywait (sem_t *s) { if (s->value) { s->value--; return 0; } errno = EAGAIN; return -1; }
int sem_wait (sem_t *s) { while (!s->value) block_on (s, 0, 0); s->value--; return 0; }
int sem_timedwait (sem_t *s, const struct timespec *abs)
{
  struct timespec now;
  long long d;
  unsigned long deadline;
  clock_gettime (CLOCK_REALTIME, &now);
  d = ((long long) abs->tv_sec - now.tv_sec) * 1000000000LL + (abs->tv_nsec - now.tv_nsec);
  if (d < 0) d = 0;
  deadline = now_cs () + (unsigned long) ((d + 9999999LL) / 10000000LL);
  while (!s->value) if (block_on (s, 1, deadline) == ETIMEDOUT && !s->value) { errno = ETIMEDOUT; return -1; }
  s->value--;
  return 0;
}
int sem_getvalue (sem_t *s, int *v) { *v = (int) s->value; return 0; }

/* ---- dlopen: still no dynamic loading ---- */
#include <dlfcn.h>
void *dlopen (const char *f, int fl) { (void) f; (void) fl; return 0; }
void *dlsym (void *h, const char *s) { (void) h; (void) s; return 0; }
int dlclose (void *h) { (void) h; return -1; }
char *dlerror (void) { return (char *) "dynamic loading is not available on RISC OS 64 (programs are statically linked)"; }
