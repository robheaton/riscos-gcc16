/* t_timer.c - POSIX timers (signal and thread notification, one-shot and periodic, gettime, delete) and named semaphores. */
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf ("FAIL %s (line %d, errno %d)\n", #c, __LINE__, errno); } else printf ("ok   %s\n", #c); } while (0)

static volatile int hits, thread_hits, thread_val;
static void on_sig (int s) { (void) s; hits++; }
static void on_thread (union sigval v) { thread_hits++; thread_val = v.sival_int; }
static void nap (int ms) { struct timespec t = { 0, ms * 1000000L }; nanosleep (&t, 0); }
static void wait_for (volatile int *p, int n, int max_ms) { int i; for (i = 0; i < max_ms / 20 && *p < n; i++) nap (20); }

int main (void)
{
  timer_t t1, t2, t3;
  struct sigevent ev;
  struct itimerspec its, got;
  sem_t *s, *s2;
  signal (SIGUSR1, on_sig);
  memset (&ev, 0, sizeof ev);
  ev.sigev_notify = SIGEV_SIGNAL; ev.sigev_signo = SIGUSR1;
  CHECK (timer_create (CLOCK_REALTIME, &ev, &t1) == 0);
  memset (&its, 0, sizeof its);
  its.it_value.tv_nsec = 200000000L;
  CHECK (timer_settime (t1, 0, &its, 0) == 0);
  CHECK (timer_gettime (t1, &got) == 0 && (got.it_value.tv_sec * 1000 + got.it_value.tv_nsec / 1000000) > 0 && got.it_value.tv_nsec <= 200000000L);
  nap (20);
  CHECK (hits == 0);
  wait_for (&hits, 1, 2000);
  CHECK (hits == 1);
  nap (300);
  CHECK (hits == 1);					/* one-shot */
  CHECK (timer_gettime (t1, &got) == 0 && got.it_value.tv_sec == 0 && got.it_value.tv_nsec == 0);
  /* periodic, with a thread notification */
  memset (&ev, 0, sizeof ev);
  ev.sigev_notify = SIGEV_THREAD; ev.sigev_notify_function = on_thread; ev.sigev_value.sival_int = 42;
  CHECK (timer_create (CLOCK_MONOTONIC, &ev, &t2) == 0);
  its.it_value.tv_nsec = 100000000L; its.it_interval.tv_nsec = 100000000L;
  CHECK (timer_settime (t2, 0, &its, 0) == 0);
  wait_for (&thread_hits, 3, 3000);
  CHECK (thread_hits >= 3 && thread_val == 42);
  memset (&its, 0, sizeof its);
  CHECK (timer_settime (t2, 0, &its, 0) == 0);		/* disarm */
  { int n = thread_hits; nap (400); CHECK (thread_hits == n); }
  CHECK (timer_delete (t2) == 0 && timer_delete (t1) == 0);
  errno = 0;
  CHECK (timer_delete (t1) == -1 && errno == EINVAL);
  CHECK (timer_create (CLOCK_REALTIME, 0, &t3) == 0 && timer_delete (t3) == 0);
  /* named semaphores */
  errno = 0;
  CHECK (sem_open ("/nosuch", 0) == SEM_FAILED && errno == ENOENT);
  errno = 0;
  CHECK (sem_open ("badname", O_CREAT, 0644, 0) == SEM_FAILED && errno == EINVAL);
  s = sem_open ("/wxsem", O_CREAT, 0644, 1);
  CHECK (s != SEM_FAILED);
  errno = 0;
  CHECK (sem_open ("/wxsem", O_CREAT | O_EXCL, 0644, 1) == SEM_FAILED && errno == EEXIST);
  s2 = sem_open ("/wxsem", 0);
  CHECK (s2 == s);
  CHECK (sem_trywait (s) == 0);
  errno = 0;
  CHECK (sem_trywait (s2) == -1 && errno == EAGAIN);
  CHECK (sem_post (s) == 0 && sem_wait (s2) == 0);
  CHECK (sem_close (s2) == 0);
  CHECK (sem_unlink ("/wxsem") == 0);
  errno = 0;
  CHECK (sem_open ("/wxsem", 0) == SEM_FAILED && errno == ENOENT);
  errno = 0;
  CHECK (sem_unlink ("/wxsem") == -1 && errno == ENOENT);
  CHECK (sem_close (s) == 0);
  printf (fails ? "FAILED: %d failures\n" : "PASSED: %d failures\n", fails);
  return fails != 0;
}
