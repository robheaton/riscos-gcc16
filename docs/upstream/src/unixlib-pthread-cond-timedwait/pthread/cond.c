/* Condition variables
   Written by Alex Waugh.
   Copyright (c) 2002, 2003, 2004, 2005, 2006, 2020 UnixLib Developers.  */

#include <errno.h>
#include <time.h>
#include <sys/time.h>

#include <internal/os.h>
#include <pthread.h>

#ifdef PTHREAD_DEBUG
#  include <sys/debug.h>
#endif

/* Initialise a condition variable with the given attributes */
int
pthread_cond_init (pthread_cond_t *cond, const pthread_condattr_t *attr)
{
  if (cond == NULL)
    return EINVAL;

  cond->waiting = NULL;
  cond->clock_id = attr ? attr->clock_id : CLOCK_REALTIME;

  return 0;
}

/* Destroy a condition variable */
int
pthread_cond_destroy (pthread_cond_t *cond)
{
  if (cond == NULL)
    return EINVAL;

  return 0;
}

/* Wait for a condition variable to be signalled */
int
pthread_cond_wait (pthread_cond_t *cond, pthread_mutex_t *mutex)
{
  return pthread_cond_timedwait (cond, mutex, NULL);
}

/* The scheduler (__pthread_context_switch) ends a timed wait once clock ()
   has passed the thread's condtimeout, and clock () counts centiseconds.
   Turn ABSTIME, an absolute time on the clock CLOCK_ID of the condition
   variable, into such a deadline.

   The old code took the difference between ABSTIME and time (), which only
   has a resolution of one second, so every wait overran by the fractional
   part of the current second (up to a second); for a CLOCK_MONOTONIC
   condition variable it even added clock () to a time that was already
   absolute.  Both clocks have centisecond resolution, so the difference is
   exact up to rounding, and it is rounded UP: a wait may be a little late
   but is never early.

   Returns 0 and stores the deadline in *DEADLINE, ETIMEDOUT if ABSTIME has
   already passed, or EINVAL.  */
static int
cond_deadline (clockid_t clock_id, const struct timespec *abstime,
               clock_t *deadline)
{
  struct timespec now;
  long long secs;
  long nsecs, cs;

  if (abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000L)
    return EINVAL;

  if (clock_gettime (clock_id, &now) != 0)
    return EINVAL;

  /* Time remaining: secs + nsecs / 10^9, with 0 <= nsecs < 10^9.  */
  secs = (long long) abstime->tv_sec - now.tv_sec;
  nsecs = abstime->tv_nsec - now.tv_nsec;
  if (nsecs < 0)
    {
      secs--;
      nsecs += 1000000000L;
    }
  if (secs < 0 || (secs == 0 && nsecs == 0))
    return ETIMEDOUT;

  /* Whole centiseconds, rounded up.  A wait of more than 10^7 seconds (115
     days) is cut short rather than overflowing clock_t.  */
  if (secs > 10000000)
    cs = 1000000000L;
  else
    cs = (long) secs * 100 + (nsecs + 9999999) / 10000000;

  *deadline = (clock_t) ((unsigned long) clock () + (unsigned long) cs);
  return 0;
}

/* Wait for a condition variable to be signalled, with a timeout */
int
pthread_cond_timedwait (pthread_cond_t *cond, pthread_mutex_t *mutex,
                        const struct timespec *abstime)
{
  pthread_t thread;
  clock_t timeout = 0;

  if (cond == NULL || mutex == NULL)
    return EINVAL;

  if (mutex->owner != __pthread_running_thread)
    return EINVAL;

  if (abstime != NULL)
    {
      /* Nothing has been changed yet, so a deadline that has already
         passed (or an invalid one) can fail with the mutex still held.  */
      int err = cond_deadline (cond->clock_id, abstime, &timeout);
      if (err)
        return err;
    }

  __pthread_disable_ints ();

  /* This call will not return an error unless mutex is NULL, which we've
     already checked for */
  __pthread_lock_unlock (mutex,0);

  __pthread_running_thread->cond = cond;
  __pthread_running_thread->nextwait = NULL;

  if (abstime == NULL)
    {
      /* No timeout */
      __pthread_running_thread->state = STATE_COND_WAIT;
      __pthread_running_thread->condtimeout = 0;
    }
  else
    {
      __pthread_running_thread->state = STATE_COND_TIMED_WAIT;
      __pthread_running_thread->condtimeout = timeout;
    }

  /* Add this thread to the linked list of threads that are waiting on
     this condition var */
  thread = cond->waiting;
  if (thread == NULL)
    {
      cond->waiting = __pthread_running_thread;
    }
  else
    {
      while (thread->nextwait != NULL)
        thread = thread->nextwait;

      thread->nextwait = __pthread_running_thread;
    }

  __pthread_enable_ints ();

  pthread_yield ();
  /* pthread_yield won't return until either the condition var has been signalled, or a timeout occured */

  if (abstime != NULL)
    {
      if (clock () > __pthread_running_thread->condtimeout)
        {
          pthread_mutex_lock (mutex);
          return ETIMEDOUT;
        }
    }

  return pthread_mutex_lock (mutex);
}

/* Signal the specified condition var */
int
pthread_cond_signal (pthread_cond_t *cond)
{
  pthread_t thread;

  if (cond == NULL)
    return EINVAL;

  __pthread_disable_ints ();

  /* Set the first thread waiting on the condition var to a runnable state */
  thread = cond->waiting;
  if (thread != NULL)
    {
#ifdef PTHREAD_DEBUG
      debug_printf ("-- pthread_cond_signal: Signalling thread %p, next waiting = %p\n", thread, thread->nextwait);
#endif
      thread->state = STATE_RUNNING;
      cond->waiting = thread->nextwait;
    }
#ifdef PTHREAD_DEBUG
  else
    debug_printf ("No thread to signal\n");
#endif

  __pthread_enable_ints ();
  return 0;
}

/* Broadcast a signal to all threads waiting on this conition var */
int pthread_cond_broadcast(pthread_cond_t *cond)
{
  pthread_t thread;

  if (cond == NULL)
    return EINVAL;

  __pthread_disable_ints ();

  thread = cond->waiting;

  /* Set all threads waiting on the condition var to a runnable state */
  while (thread != NULL)
    {
      thread->state = STATE_RUNNING;
      thread = thread->nextwait;
    }

  cond->waiting = NULL;

  __pthread_enable_ints ();
  return 0;
}

