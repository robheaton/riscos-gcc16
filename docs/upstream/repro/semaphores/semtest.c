/* semtest: POSIX semaphores and threads.  Built for RISC OS (UnixLib) and for a Linux host (glibc): the checks are the same, so a failure on one and not the other is the library.
   In UnixLib of trunk r7800 sem_wait polls (burning the time slice), leaks a queue entry per call, and sem_timedwait is ENOSYS.  */
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static int checks, fails;
static void check (int ok, const char *what)
{
  checks++;
  if (!ok) fails++;
  printf ("  %-4s %s\n", ok ? "ok" : "FAIL", what);
  fflush (stdout);
}

static long long now_ms (void)
{
  struct timeval tv;
  gettimeofday (&tv, NULL);
  return (long long) tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static struct timespec deadline_in (long ms)           /* absolute CLOCK_REALTIME time ms from now (ms may be negative) */
{
  struct timespec ts;
  long long t = now_ms () + ms;
  ts.tv_sec = (time_t) (t / 1000);
  ts.tv_nsec = (long) (t % 1000) * 1000000L;
  if (ts.tv_nsec < 0) { ts.tv_nsec += 1000000000L; ts.tv_sec--; }
  return ts;
}

static void ms_sleep (long ms) { usleep (ms * 1000); }

static sem_t s;
static volatile int done;
static long long t_done;

static void *waiter (void *arg)
{
  (void) arg;
  sem_wait (&s);
  t_done = now_ms ();
  done = 1;
  return NULL;
}

static void *delayed_post (void *arg)
{
  ms_sleep ((long) (size_t) arg);
  sem_post (&s);
  return NULL;
}

static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static int woken;
static void *counting_waiter (void *arg)
{
  (void) arg;
  sem_wait (&s);
  pthread_mutex_lock (&m);
  woken++;
  pthread_mutex_unlock (&m);
  return NULL;
}

static sem_t ping, pong;
static void *ponger (void *arg)
{
  int i, n = (int) (size_t) arg;
  for (i = 0; i < n; i++)
    {
      sem_wait (&ping);
      sem_post (&pong);
    }
  return NULL;
}

int main (void)
{
  pthread_t th[4];
  long long t0, t1;
  struct timespec ts;
  int v, i, rc;

  puts ("== 1. counting without threads");
  check (sem_init (&s, 0, 0) == 0, "sem_init (0)");
  errno = 0;
  check (sem_trywait (&s) == -1 && errno == EAGAIN, "sem_trywait on an empty semaphore: EAGAIN");
  check (sem_post (&s) == 0 && sem_getvalue (&s, &v) == 0 && v == 1, "sem_post makes the value 1");
  check (sem_trywait (&s) == 0 && sem_getvalue (&s, &v) == 0 && v == 0, "sem_trywait takes it");
  for (i = 0; i < 5; i++) sem_post (&s);
  sem_getvalue (&s, &v);
  check (v == 5, "five posts: the value is 5");
  for (i = 0; i < 5; i++) if (sem_trywait (&s) != 0) break;
  check (i == 5 && sem_trywait (&s) == -1, "five trywaits succeed, the sixth fails");

  puts ("== 2. sem_wait blocks until the post");
  done = 0;
  pthread_create (&th[0], NULL, waiter, NULL);
  ms_sleep (200);
  check (done == 0, "after 200 ms the waiting thread is still blocked");
  t0 = now_ms ();
  sem_post (&s);
  pthread_join (th[0], NULL);
  check (done == 1 && t_done - t0 < 1000, "the post releases it");

  puts ("== 3. sem_timedwait");
  ts = deadline_in (300);
  t0 = now_ms ();
  errno = 0;
  rc = sem_timedwait (&s, &ts);
  t1 = now_ms ();
  printf ("  (timed out after %lld ms, asked for 300)\n", t1 - t0);
  check (rc == -1 && errno == ETIMEDOUT, "an empty semaphore times out with ETIMEDOUT");
  check (t1 - t0 >= 250 && t1 - t0 <= 900, "after about 300 ms");

  pthread_create (&th[0], NULL, delayed_post, (void *) (size_t) 100);
  ts = deadline_in (3000);
  t0 = now_ms ();
  rc = sem_timedwait (&s, &ts);
  t1 = now_ms ();
  pthread_join (th[0], NULL);
  printf ("  (acquired after %lld ms)\n", t1 - t0);
  check (rc == 0 && t1 - t0 < 1500, "a post after 100 ms ends a 3 s wait early, with success");

  sem_post (&s);
  ts = deadline_in (-1000);
  check (sem_timedwait (&s, &ts) == 0, "an available semaphore is taken although the deadline has passed");
  t0 = now_ms ();
  errno = 0;
  rc = sem_timedwait (&s, &ts);
  check (rc == -1 && errno == ETIMEDOUT && now_ms () - t0 < 300, "an empty one with a deadline in the past fails at once with ETIMEDOUT");
  ts = deadline_in (100);
  ts.tv_nsec = 1000000000L;
  errno = 0;
  check (sem_timedwait (&s, &ts) == -1 && errno == EINVAL, "tv_nsec out of range: EINVAL");

  puts ("== 4. several waiters, one post wakes one");
  woken = 0;
  for (i = 0; i < 4; i++) pthread_create (&th[i], NULL, counting_waiter, NULL);
  ms_sleep (200);
  sem_post (&s);
  sem_post (&s);
  ms_sleep (300);
  pthread_mutex_lock (&m); v = woken; pthread_mutex_unlock (&m);
  check (v == 2, "two posts wake exactly two of four waiters");
  sem_post (&s);
  sem_post (&s);
  for (i = 0; i < 4; i++) pthread_join (th[i], NULL);
  check (woken == 4, "two more posts wake the others");

  puts ("== 5. sem_destroy with a waiter");
  pthread_create (&th[0], NULL, waiter, NULL);
  done = 0;
  ms_sleep (150);
#ifdef __riscos__
  errno = 0;
  check (sem_destroy (&s) == -1 && errno == EBUSY, "sem_destroy while a thread waits: EBUSY");
#endif
  sem_post (&s);
  pthread_join (th[0], NULL);
  check (sem_destroy (&s) == 0, "sem_destroy afterwards");

  puts ("== 6. ping-pong between two threads, 1000 round trips");
  sem_init (&ping, 0, 0);
  sem_init (&pong, 0, 0);
  pthread_create (&th[0], NULL, ponger, (void *) (size_t) 1000);
  t0 = now_ms ();
  for (i = 0; i < 1000; i++)
    {
      sem_post (&ping);
      ts = deadline_in (10000);
      if (sem_timedwait (&pong, &ts) != 0) break;
    }
  t1 = now_ms ();
  printf ("  (%d round trips in %lld ms)\n", i, t1 - t0);
  check (i == 1000, "all round trips complete");
  if (i == 1000) pthread_join (th[0], NULL);
  sem_destroy (&ping);
  sem_destroy (&pong);

#ifdef __riscos__
  puts ("== 7. no memory leak");
  {
    char *before, *after;
    sem_init (&s, 0, 1);
    before = sbrk (0);
    for (i = 0; i < 100000; i++)
      {
        sem_wait (&s);
        sem_post (&s);
      }
    after = sbrk (0);
    printf ("  (the heap grew by %ld bytes over 100000 sem_wait calls; the code of trunk leaks a queue entry per call)\n", (long) (after - before));
    check (after - before < 65536, "sem_wait does not leak");
    sem_destroy (&s);
  }
#endif

  printf ("SUMMARY [semtest]: %d checks, %d failed -> %s\n", checks, fails, fails ? "FAIL" : "PASS");
  return fails ? 1 : 0;
}
