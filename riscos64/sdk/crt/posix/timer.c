/* timer.c - POSIX timers (timer_create, timer_settime, timer_gettime, timer_getoverrun, timer_delete) and named semaphores (sem_open, sem_close, sem_unlink).  There are no interrupts: a timer expires at the
   places where the library looks for signals (every read and write, sleeps, poll and select, thread waits: __ul64_poll_signals), in centiseconds like alarm ().  SIGEV_SIGNAL raises the signal there,
   SIGEV_THREAD calls the function there (in the calling thread), SIGEV_NONE only counts.  Named semaphores live in this program only (there are no other processes to share them with).  The SDK of RISC OS 64. */
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);

#define NTIMERS 32
static struct timer { int used; int notify, signo; union sigval value; void (*fn) (union sigval); unsigned long at, interval; int armed, overrun; } timers[NTIMERS];

static unsigned long now_cs (void)
{
  unsigned long r[10] = { 0 };
  swi_x (0x42, r);
  return r[0] & 0xFFFFFFFFUL;
}
static int due (unsigned long at, unsigned long now) { return ((now - at) & 0xFFFFFFFFUL) < 0x80000000UL; }

int timer_create (clockid_t clock, struct sigevent *__restrict ev, timer_t *__restrict id)
{
  int i;
  if (clock != CLOCK_REALTIME && clock != CLOCK_MONOTONIC) { errno = EINVAL; return -1; }
  for (i = 0; i < NTIMERS; i++) if (!timers[i].used) break;
  if (i == NTIMERS) { errno = EAGAIN; return -1; }
  memset (&timers[i], 0, sizeof timers[i]);
  timers[i].used = 1;
  if (ev) { timers[i].notify = ev->sigev_notify; timers[i].signo = ev->sigev_signo; timers[i].value = ev->sigev_value; timers[i].fn = ev->sigev_notify_function; }
  else { timers[i].notify = SIGEV_SIGNAL; timers[i].signo = SIGALRM; timers[i].value.sival_int = i; }
  if (timers[i].notify == SIGEV_SIGNAL && (timers[i].signo <= 0 || timers[i].signo >= NSIG)) { timers[i].used = 0; errno = EINVAL; return -1; }
  *id = (timer_t) (i + 1);
  return 0;
}

static struct timer *get (timer_t id)
{
  unsigned long i = (unsigned long) id;
  if (i < 1 || i > NTIMERS || !timers[i - 1].used) { errno = EINVAL; return 0; }
  return &timers[i - 1];
}

int timer_delete (timer_t id) { struct timer *t = get (id); if (!t) return -1; t->used = 0; t->armed = 0; return 0; }

int timer_settime (timer_t id, int flags, const struct itimerspec *__restrict n, struct itimerspec *__restrict old)
{
  struct timer *t = get (id);
  unsigned long now = now_cs (), v, iv;
  if (!t) return -1;
  if (old) timer_gettime (id, old);
  if (n->it_value.tv_nsec < 0 || n->it_value.tv_nsec >= 1000000000L || n->it_interval.tv_nsec < 0 || n->it_interval.tv_nsec >= 1000000000L) { errno = EINVAL; return -1; }
  v = (unsigned long) n->it_value.tv_sec * 100 + (unsigned long) ((n->it_value.tv_nsec + 9999999) / 10000000);
  iv = (unsigned long) n->it_interval.tv_sec * 100 + (unsigned long) ((n->it_interval.tv_nsec + 9999999) / 10000000);
  t->overrun = 0;
  if (n->it_value.tv_sec == 0 && n->it_value.tv_nsec == 0) { t->armed = 0; return 0; }
  if (flags & TIMER_ABSTIME)
    {
      struct timespec cur;
      long d;
      clock_gettime (CLOCK_REALTIME, &cur);
      d = (long) (n->it_value.tv_sec - cur.tv_sec) * 100 + (n->it_value.tv_nsec - cur.tv_nsec) / 10000000;
      v = d > 0 ? (unsigned long) d : 1;
    }
  if (v == 0) v = 1;
  t->at = now + v; t->interval = iv; t->armed = 1;
  return 0;
}

int timer_gettime (timer_t id, struct itimerspec *o)
{
  struct timer *t = get (id);
  unsigned long d = 0;
  if (!t) return -1;
  if (t->armed) { d = (t->at - now_cs ()) & 0xFFFFFFFFUL; if (d > 0x40000000UL) d = 0; }
  o->it_value.tv_sec = (time_t) (d / 100); o->it_value.tv_nsec = (long) (d % 100) * 10000000L;
  o->it_interval.tv_sec = (time_t) (t->interval / 100); o->it_interval.tv_nsec = (long) (t->interval % 100) * 10000000L;
  return 0;
}

int timer_getoverrun (timer_t id) { struct timer *t = get (id); return t ? t->overrun : -1; }

/* called from __ul64_poll_signals: the timers that have run out (handlers run here, so after the table is updated) */
void __ul64_poll_timers (void)
{
  int i;
  unsigned long now = now_cs ();
  for (i = 0; i < NTIMERS; i++)
    {
      struct timer *t = &timers[i];
      if (!t->used || !t->armed || !due (t->at, now)) continue;
      if (t->interval)
	{
	  unsigned long late = (now - t->at) / t->interval;
	  t->overrun = (int) late;
	  t->at += (late + 1) * t->interval;
	}
      else t->armed = 0;
      if (t->notify == SIGEV_SIGNAL) raise (t->signo);
      else if (t->notify == SIGEV_THREAD && t->fn) t->fn (t->value);
    }
}

/* ---- named semaphores ---- */
#define NSEMS 16
static struct { char *name; sem_t *s; int refs; int unlinked; } named[NSEMS];

sem_t *sem_open (const char *name, int oflag, ...)
{
  int i, free_slot = -1;
  unsigned mode = 0, value = 0;
  if (!name || name[0] != '/' || !name[1]) { errno = EINVAL; return SEM_FAILED; }
  if (oflag & O_CREAT) { va_list ap; va_start (ap, oflag); mode = va_arg (ap, unsigned); value = va_arg (ap, unsigned); va_end (ap); }
  (void) mode;
  for (i = 0; i < NSEMS; i++)
    {
      if (!named[i].name) { if (free_slot < 0) free_slot = i; continue; }
      if (!named[i].unlinked && !strcmp (named[i].name, name))
	{
	  if ((oflag & O_CREAT) && (oflag & O_EXCL)) { errno = EEXIST; return SEM_FAILED; }
	  named[i].refs++;
	  return named[i].s;
	}
    }
  if (!(oflag & O_CREAT)) { errno = ENOENT; return SEM_FAILED; }
  if (value > (unsigned) SEM_VALUE_MAX) { errno = EINVAL; return SEM_FAILED; }
  if (free_slot < 0) { errno = ENFILE; return SEM_FAILED; }
  named[free_slot].s = malloc (sizeof (sem_t));
  named[free_slot].name = strdup (name);
  if (!named[free_slot].s || !named[free_slot].name) { free (named[free_slot].s); free (named[free_slot].name); named[free_slot].name = 0; errno = ENOMEM; return SEM_FAILED; }
  sem_init (named[free_slot].s, 1, value);
  named[free_slot].refs = 1; named[free_slot].unlinked = 0;
  return named[free_slot].s;
}

static void drop (int i) { free (named[i].name); free (named[i].s); named[i].name = 0; named[i].s = 0; named[i].refs = 0; }

int sem_close (sem_t *s)
{
  int i;
  for (i = 0; i < NSEMS; i++)
    if (named[i].name && named[i].s == s)
      {
	if (--named[i].refs <= 0 && named[i].unlinked) drop (i);
	return 0;
      }
  errno = EINVAL;
  return -1;
}

int sem_unlink (const char *name)
{
  int i;
  for (i = 0; i < NSEMS; i++)
    if (named[i].name && !named[i].unlinked && !strcmp (named[i].name, name))
      {
	named[i].unlinked = 1;
	if (named[i].refs <= 0) drop (i);
	return 0;
      }
  errno = ENOENT;
  return -1;
}
