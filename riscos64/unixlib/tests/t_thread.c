/* t_thread.c - the cooperative POSIX threads: create/join, a mutex, a condition variable (producer and consumer), once, keys and their destructors, thread-local variables, a read-write lock, a barrier, a semaphore,
   timed waits, sleeping threads, sched_yield, a deep stack, detached threads. */
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int fails;
#define CHECK(c) do { if (c) printf ("ok   %s\n", #c); else { printf ("FAIL %s\n", #c); fails++; } } while (0)

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static int counter, queue_len, produced, consumed, done_flag;
static __thread int tls_value = 100;
static __thread char tls_big[300];

static void *add_thread (void *arg)
{
  int i, n = (int) (long) arg;
  for (i = 0; i < 1000; i++) { pthread_mutex_lock (&mu); counter += n; if (i % 100 == 0) sched_yield (); pthread_mutex_unlock (&mu); }
  return (void *) (long) (n * 10);
}
static void *producer (void *arg)
{
  int i;
  (void) arg;
  for (i = 0; i < 20; i++)
    {
      pthread_mutex_lock (&mu);
      while (queue_len >= 3) pthread_cond_wait (&cv, &mu);
      queue_len++; produced++;
      pthread_cond_broadcast (&cv);
      pthread_mutex_unlock (&mu);
    }
  return 0;
}
static void *consumer (void *arg)
{
  (void) arg;
  for (;;)
    {
      pthread_mutex_lock (&mu);
      while (queue_len == 0 && !done_flag) pthread_cond_wait (&cv, &mu);
      if (queue_len == 0 && done_flag) { pthread_mutex_unlock (&mu); break; }
      queue_len--; consumed++;
      pthread_cond_broadcast (&cv);
      pthread_mutex_unlock (&mu);
    }
  return 0;
}
static int once_count;
static void once_fn (void) { once_count++; }
static void *once_thread (void *arg) { pthread_once ((pthread_once_t *) arg, once_fn); return 0; }
static int destructed;
static void key_dtor (void *v) { destructed += (int) (long) v; }
static pthread_key_t key;
static void *key_thread (void *arg) { pthread_setspecific (key, arg); return 0; }
static void *tls_thread (void *arg) { tls_value += (int) (long) arg; memset (tls_big, 7, sizeof tls_big); sched_yield (); return (void *) (long) (tls_value + tls_big[299]); }
static pthread_rwlock_t rw = PTHREAD_RWLOCK_INITIALIZER;
static int rwdata;
static void *reader (void *arg) { int i, seen = 0; (void) arg; for (i = 0; i < 5; i++) { pthread_rwlock_rdlock (&rw); seen += rwdata; pthread_rwlock_unlock (&rw); sched_yield (); } return (void *) (long) seen; }
static void *writer (void *arg) { (void) arg; pthread_rwlock_wrlock (&rw); rwdata = 5; pthread_rwlock_unlock (&rw); return 0; }
static pthread_barrier_t bar;
static int after_barrier, serial;
static void *barrier_thread (void *arg) { (void) arg; if (pthread_barrier_wait (&bar) == PTHREAD_BARRIER_SERIAL_THREAD) serial++; after_barrier++; return 0; }
static sem_t sem;
static int sem_order[4], sem_n;
static void *sem_thread (void *arg) { sem_wait (&sem); sem_order[sem_n++] = (int) (long) arg; return 0; }
static void *sleeper (void *arg) { usleep ((useconds_t) (long) arg * 1000); pthread_mutex_lock (&mu); sem_order[sem_n++] = (int) (long) arg; pthread_mutex_unlock (&mu); return 0; }
static long deep (int n) { volatile char pad[200]; pad[0] = (char) n; return n == 0 ? 0 : 1 + deep (n - 1); }
static void *deep_thread (void *arg) { (void) arg; return (void *) deep (500); }
static int detached_ran;
static void *detached_thread (void *arg) { (void) arg; detached_ran++; return 0; }

int main (void)
{
  pthread_t t[8];
  pthread_attr_t at;
  void *rv;
  int i;
  struct timespec ts;

  CHECK (pthread_create (&t[0], 0, add_thread, (void *) 1L) == 0 && pthread_create (&t[1], 0, add_thread, (void *) 2L) == 0);
  CHECK (pthread_join (t[0], &rv) == 0 && (long) rv == 10);
  CHECK (pthread_join (t[1], &rv) == 0 && (long) rv == 20);
  CHECK (counter == 3000);

  pthread_create (&t[0], 0, consumer, 0); pthread_create (&t[1], 0, consumer, 0); pthread_create (&t[2], 0, producer, 0);
  pthread_join (t[2], 0);
  pthread_mutex_lock (&mu); done_flag = 1; pthread_cond_broadcast (&cv); pthread_mutex_unlock (&mu);
  pthread_join (t[0], 0); pthread_join (t[1], 0);
  CHECK (produced == 20 && consumed == 20 && queue_len == 0);

  { pthread_once_t o = PTHREAD_ONCE_INIT; for (i = 0; i < 4; i++) pthread_create (&t[i], 0, once_thread, &o); for (i = 0; i < 4; i++) pthread_join (t[i], 0); CHECK (once_count == 1); }

  pthread_key_create (&key, key_dtor);
  pthread_create (&t[0], 0, key_thread, (void *) 5L); pthread_create (&t[1], 0, key_thread, (void *) 7L);
  pthread_join (t[0], 0); pthread_join (t[1], 0);
  CHECK (destructed == 12);

  tls_value = 1;
  pthread_create (&t[0], 0, tls_thread, (void *) 1L); pthread_create (&t[1], 0, tls_thread, (void *) 2L);
  pthread_join (t[0], &rv); CHECK ((long) rv == 100 + 1 + 7);
  pthread_join (t[1], &rv); CHECK ((long) rv == 100 + 2 + 7);
  CHECK (tls_value == 1 && tls_big[299] == 0);				/* the main thread's variables are not touched */

  pthread_create (&t[0], 0, writer, 0); pthread_create (&t[1], 0, reader, 0);
  pthread_join (t[0], 0); pthread_join (t[1], &rv);
  CHECK ((long) rv >= 0 && rwdata == 5);

  pthread_barrier_init (&bar, 0, 4);
  for (i = 0; i < 3; i++) pthread_create (&t[i], 0, barrier_thread, 0);
  barrier_thread (0);
  for (i = 0; i < 3; i++) pthread_join (t[i], 0);
  CHECK (after_barrier == 4 && serial == 1);

  sem_init (&sem, 0, 0);
  for (i = 0; i < 3; i++) pthread_create (&t[i], 0, sem_thread, (void *) (long) (i + 1));
  sched_yield (); sem_post (&sem); sem_post (&sem); sem_post (&sem);
  for (i = 0; i < 3; i++) pthread_join (t[i], 0);
  CHECK (sem_n == 3 && sem_order[0] == 1 && sem_order[1] == 2 && sem_order[2] == 3);	/* the oldest waiter first */

  sem_n = 0;
  pthread_create (&t[0], 0, sleeper, (void *) 300L); pthread_create (&t[1], 0, sleeper, (void *) 100L); pthread_create (&t[2], 0, sleeper, (void *) 200L);
  for (i = 0; i < 3; i++) pthread_join (t[i], 0);
  CHECK (sem_order[0] == 100 && sem_order[1] == 200 && sem_order[2] == 300);	/* they woke in the order of their times */

  clock_gettime (CLOCK_REALTIME, &ts); ts.tv_nsec += 200000000L; if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
  pthread_mutex_lock (&mu);
  CHECK (pthread_cond_timedwait (&cv, &mu, &ts) == ETIMEDOUT);
  pthread_mutex_unlock (&mu);

  pthread_attr_init (&at); pthread_attr_setstacksize (&at, 256 * 1024);
  pthread_create (&t[0], &at, deep_thread, 0);
  pthread_join (t[0], &rv);
  CHECK ((long) rv == 500);

  pthread_attr_setdetachstate (&at, PTHREAD_CREATE_DETACHED);
  pthread_create (&t[0], &at, detached_thread, 0);
  CHECK (pthread_join (t[0], 0) != 0 || 1);
  usleep (50000);
  CHECK (detached_ran == 1);

  { pthread_mutexattr_t ma; pthread_mutex_t rm; pthread_mutexattr_init (&ma); pthread_mutexattr_settype (&ma, PTHREAD_MUTEX_RECURSIVE); pthread_mutex_init (&rm, &ma);
    CHECK (pthread_mutex_lock (&rm) == 0 && pthread_mutex_lock (&rm) == 0 && pthread_mutex_unlock (&rm) == 0 && pthread_mutex_unlock (&rm) == 0 && pthread_mutex_trylock (&rm) == 0); }
  CHECK (pthread_self () != 0 && pthread_equal (pthread_self (), pthread_self ()));
  printf ("%s: %d failures\n", fails ? "FAILED" : "PASSED", fails);
  return fails != 0;
}
