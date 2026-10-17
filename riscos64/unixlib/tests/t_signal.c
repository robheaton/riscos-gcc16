/* t_signal.c - signals that arrive: alarm () as SIGALRM (pause, sleep, blocked and unblocked), raise of a blocked signal.  Escape as SIGINT cannot be tested without a keyboard (try it by hand with sigint_demo). */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static int fails;
static volatile int got_alrm, got_usr1;
#define CHECK(c) do { if (c) printf ("ok   %s\n", #c); else { printf ("FAIL %s\n", #c); fails++; } } while (0)
static void on_alrm (int s) { (void) s; got_alrm++; }
static void on_usr1 (int s) { (void) s; got_usr1++; }

int main (void)
{
  sigset_t set, old;
  struct sigaction sa;
  time_t t0;
  int r;

  sa.sa_handler = on_alrm; sigemptyset (&sa.sa_mask); sa.sa_flags = SA_RESTART;
  sigaction (SIGALRM, &sa, 0);
  sa.sa_handler = on_usr1;
  sigaction (SIGUSR1, &sa, 0);

  alarm (1);
  r = pause ();
  CHECK (r == -1 && errno == EINTR && got_alrm == 1);

  alarm (1);
  t0 = time (0);
  r = sleep (3);						/* ended early by the alarm */
  CHECK (got_alrm == 2 && r > 0 && time (0) - t0 <= 2);

  CHECK (alarm (5) == 0);
  CHECK (alarm (0) >= 4);					/* the seconds left, and cancelled */

  sigemptyset (&set); sigaddset (&set, SIGUSR1);
  sigprocmask (SIG_BLOCK, &set, &old);
  raise (SIGUSR1);
  CHECK (got_usr1 == 0);					/* pending while blocked */
  sigprocmask (SIG_SETMASK, &old, 0);
  CHECK (got_usr1 == 1);					/* delivered on unblock */

  sigemptyset (&set); sigaddset (&set, SIGALRM);
  sigprocmask (SIG_BLOCK, &set, &old);
  alarm (1);
  usleep (1500000);
  CHECK (got_alrm == 2);					/* the alarm went off, but is held */
  sigprocmask (SIG_SETMASK, &old, 0);
  CHECK (got_alrm == 3);
  printf ("%s: %d failures\n", fails ? "FAILED" : "PASSED", fails);
  return fails != 0;
}
