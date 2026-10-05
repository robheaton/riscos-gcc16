/*
 * File taken from glibc.
 *  - SCL poison added.
 */
#ifdef __TARGET_SCL__
#  error "SCL build should not use (L)GPL code."
#endif

/* Copyright (C) 1991, 1992, 1993, 1996, 1997 Free Software Foundation, Inc.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include <internal/unix.h>
#include <pthread.h>

/* #define DEBUG */
#ifdef DEBUG
#  include <sys/debug.h>
#endif

#define USECS_PER_CLOCK (1000000 / CLOCKS_PER_SEC)
#define NSECS_PER_CLOCK (1000000000 / CLOCKS_PER_SEC)

/* SIGALRM signal handler for `sleep'.  This does nothing but return,
   but SIG_IGN isn't supposed to break `pause'.  */
static void
sleep_handler (int sig)
{
  sig = sig;
  return;
}

/* Make the process sleep for CLOCKTICKS clock ticks, or until a signal
   arrives and is not ignored.  The function returns the number of clock
   ticks less than CLOCKTICKS which it actually slept (zero if it slept
   the full time).
   If a signal handler does a `longjmp' or modifies the handling of the
   SIGALRM signal while inside `sleep' call, the handling of the SIGALRM
   signal afterwards is undefined.  There is no return value to indicate
   error, but if `sleep' returns SECONDS, it probably didn't work.  */
static clock_t
sleep_int (clock_t clockticks)
{
  clock_t before, after, slept, remaining;
  sigset_t set, oset;
  struct sigaction act, oact;
  int save = errno;

  if (clockticks == 0)
    return 0;

  /* alarm() does not work in a TaskWindow nor whilst running as a
     WIMP program.

     Nor can several threads sleep with it: there is one alarm per process
     and one 'sleeping' flag for sigsuspend to wait on, so when a second
     thread starts to sleep its alarm replaces the first one, and when that
     goes off every thread in sigsuspend wakes up, the longer sleepers too
     early.  With other threads around, wait the same way as in a WIMP task
     instead: let the other threads run until the time is up.  */
  if (__get_taskhandle () != 0 || __ul_global.pthread_num_running_threads > 1)
    {
      before = clock ();
      while ((clock_t) ((unsigned long) clock () - (unsigned long) before)
	     < clockticks)
	pthread_yield ();
      return 0;
    }

  /* Block SIGALRM signals while frobbing the handler.  */
  if (sigemptyset (&set) < 0
      || sigaddset (&set, SIGALRM) < 0
      || sigprocmask (SIG_BLOCK, &set, &oset))
    return clockticks;

  act.sa_handler = sleep_handler;
  act.sa_flags = 0;
  act.sa_mask = oset; /* Execute handler with original mask.  */
  if (sigaction (SIGALRM, &act, &oact))
    return clockticks;

  before = clock ();
  remaining = (ualarm ((useconds_t) clockticks * USECS_PER_CLOCK, 0)
	       / USECS_PER_CLOCK);

#ifdef DEBUG
  debug_printf ("-- sleep: Set up an alarm for %d clockticks\n"
		"   Remaining: %d clockticks\n", clockticks, remaining);
#endif

  if (remaining > 0 && remaining < clockticks)
    {
      /* The user's alarm will expire before our own would.
         Restore the user's signal action state and let his alarm happen.  */
      sigaction (SIGALRM, &oact, (struct sigaction *) NULL);

      /* Restore sooner alarm.  */
      ualarm ((useconds_t) clockticks * USECS_PER_CLOCK, 0);
#ifdef DEBUG
      debug_printf ("-- sleep: A user alarm existed. Wait %d clockticks for that instead\n", remaining);
#endif
      sigsuspend (&oset);	/* Wait for it to go off.  */
#ifdef DEBUG
      debug_printf ("-- sleep: Alarm has gone off. Continuing with execution\n");
#endif
      after = clock ();
    }
  else
    {
      /* Atomically restore the old signal mask
         (which had better not block SIGALRM),
         and wait for a signal to arrive.  */
#ifdef DEBUG
      debug_printf ("-- sleep: Waiting for the alarm\n");
#endif
      sigsuspend (&oset);
#ifdef DEBUG
      debug_printf ("-- sleep: Alarm has gone off. Continuing with execution\n");
#endif
      after = clock ();

      /* Restore the old signal action state.  */
      sigaction (SIGALRM, &oact, (struct sigaction *) NULL);
    }

  /* Notice how long we actually slept.  */
  slept = after - before;

  /* Restore the user's alarm if we have not already past it.
     If we have, be sure to turn off the alarm in case a signal
     other than SIGALRM was what woke us up.  */
  (void) ualarm (remaining > slept ? (useconds_t)(remaining - slept) * USECS_PER_CLOCK : 0, 0);

  /* Restore the original signal mask.  */
  (void) sigprocmask (SIG_SETMASK, &oset, (sigset_t *) NULL);

  /* Restore the `errno' value we started with.
     Some of the calls we made might have failed, but we don't care.  */
  (void) __set_errno (save);

  return slept > clockticks ? 0 : clockticks - slept;
}

/* Make the process sleep for SECONDS seconds, or until a signal arrives
   and is not ignored.  The function returns the number of seconds less
   than SECONDS which it actually slept (zero if it slept the full time).  */
unsigned int
sleep (unsigned int seconds)
{
  PTHREAD_SAFE_CANCELLATION

  return (unsigned int) sleep_int ((clock_t)seconds * CLOCKS_PER_SEC) / CLOCKS_PER_SEC;
}


/* Sleep for time periods specified in micro-seconds.  */
int usleep (useconds_t usec)
{
  PTHREAD_SAFE_CANCELLATION

  /* An allowed & specified limitation. Otherwise our calculations might
     overflow.  */
  if (usec >= 1000000)
    __set_errno (EINVAL);

  return (int) sleep_int ((usec + USECS_PER_CLOCK-1) / USECS_PER_CLOCK) * USECS_PER_CLOCK;
}

/* Sleep for time periods specified in nanoseconds.  */
int nanosleep (const struct timespec *req, struct timespec *rem)
{
  int ticks, nticks;
  PTHREAD_SAFE_CANCELLATION;

  if (req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec > 999999999)
    __set_errno (EINVAL);

  ticks = (int) sleep_int (((req->tv_nsec + NSECS_PER_CLOCK-1)
			    / NSECS_PER_CLOCK)
			   + req->tv_sec * CLOCKS_PER_SEC);

  if (rem != NULL)
    {
      rem->tv_sec = ticks / CLOCKS_PER_SEC;
      ticks -= rem->tv_sec * CLOCKS_PER_SEC;
      rem->tv_nsec = ticks * NSECS_PER_CLOCK;
    }

  return 0;
}
