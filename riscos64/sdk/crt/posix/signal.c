/* signal.c - signal, raise, sigaction, kill (of the process itself), sigprocmask and alarm, in the way that needs no operating system support: a table of handlers, raise () calls the handler of the signal, the default of
   a signal is to end the process (or nothing, for SIGCHLD, SIGURG, SIGWINCH).  Escape and alarm () arrive at the places where the library looks for them (__ul64_poll_signals: every write, every read of the keyboard, sleeps, poll and select, pause): Escape is SIGINT, the timer of alarm () is SIGALRM.  A data abort and the like are SIGSEGV, SIGILL, SIGBUS, SIGFPE for a program that has a handler for them (fault.c).
   libriscosstub.a is searched before libc.a, so these take the place of picolibc's fallbacks.  The SDK of RISC OS 64. */
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#define NSIG_ 65
static void (*handlers[NSIG_]) (int);
static char resethand[NSIG_];					/* SA_RESETHAND was set: the handler is used once */
static sigset_t blocked;

extern void _exit (int);
extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);
static sigset_t pending;						/* signals raised while they were blocked */
static int alarm_on;
static int signalled;						/* a handler has run (for pause) */
static unsigned long alarm_at;					/* centiseconds on the monotonic clock */
#define BIT(s) ((sigset_t) 1 << (s))

extern void __ul64_fault_update (int active);
static void fault_sync (void)					/* SIGSEGV, SIGBUS, SIGILL, SIGFPE: the kernel's handlers (fault.c) are set while one of them has a handler */
{
  int active = 0, s;
  static const int fs[4] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE };
  for (s = 0; s < 4; s++) if (handlers[fs[s]] != SIG_DFL && handlers[fs[s]] != SIG_IGN && handlers[fs[s]] != 0) active = 1;
  __ul64_fault_update (active);
}
void (*__ul64_signal_handler (int sig)) (int) { return handlers[sig]; }

static int ignored_by_default (int sig) { return sig == SIGCHLD || sig == SIGURG || sig == SIGWINCH || sig == SIGCONT; }

void (*signal (int sig, void (*handler) (int))) (int)
{
  void (*old) (int);
  if (sig <= 0 || sig >= NSIG_ || sig == SIGKILL || sig == SIGSTOP) { errno = EINVAL; return SIG_ERR; }
  old = handlers[sig];
  handlers[sig] = handler;
  resethand[sig] = 0;
  if (sig == SIGSEGV || sig == SIGBUS || sig == SIGILL || sig == SIGFPE) fault_sync ();
  return old;
}

int raise (int sig)
{
  void (*h) (int);
  if (sig <= 0 || sig >= NSIG_) { errno = EINVAL; return -1; }
  h = handlers[sig];
  if (h == SIG_IGN) return 0;
  if ((blocked & BIT (sig)) && sig != SIGKILL && sig != SIGSTOP) { pending |= BIT (sig); return 0; }
  if (h == SIG_DFL || h == 0)
    {
      if (ignored_by_default (sig)) return 0;
      _exit (128 + sig);
    }
  if (resethand[sig]) { handlers[sig] = SIG_DFL; resethand[sig] = 0; }	/* only for SA_RESETHAND: signal () has the BSD meaning, as in glibc and UnixLib, and keeps the handler */
  signalled = 1;
  h (sig);
  return 0;
}

int kill (pid_t pid, int sig)
{
  if (pid != getpid () && pid != 0) { errno = ESRCH; return -1; }
  if (sig == 0) return 0;
  return raise (sig);
}

int sigaction (int sig, const struct sigaction *act, struct sigaction *old)
{
  if (sig <= 0 || sig >= NSIG_ || ((sig == SIGKILL || sig == SIGSTOP) && act)) { errno = EINVAL; return -1; }
  if (old)
    {
      old->sa_handler = handlers[sig];
      old->sa_flags = 0;
      sigemptyset (&old->sa_mask);
    }
  if (act) { handlers[sig] = act->sa_handler; resethand[sig] = (act->sa_flags & SA_RESETHAND) != 0; if (sig == SIGSEGV || sig == SIGBUS || sig == SIGILL || sig == SIGFPE) fault_sync (); }
  return 0;
}

int sigprocmask (int how, const sigset_t *set, sigset_t *old)
{
  int sig;
  if (old) *old = blocked;
  if (set)
    {
      if (how == SIG_BLOCK) blocked |= *set;
      else if (how == SIG_UNBLOCK) blocked &= ~*set;
      else blocked = *set;
      for (sig = 1; sig < NSIG_; sig++)
	if ((pending & BIT (sig)) && !(blocked & BIT (sig))) { pending &= ~BIT (sig); raise (sig); }
    }
  return 0;
}

static unsigned long now_cs (void)
{
  unsigned long r[10] = { 0 };
  swi_x (0x42, r);
  return r[0] & 0xFFFFFFFFUL;
}

/* called where the library waits or does I/O: Escape (OS_Byte 126 says it was pressed, and clears it) becomes SIGINT, the end of the alarm () time SIGALRM */
extern void __ul64_poll_timers (void) __attribute__ ((weak));
void __ul64_poll_signals (void)
{
  static int busy;
  int esc = 0, alrm = 0;
  unsigned long r[10] = { 126 };
  if (busy) return;
  busy = 1;
  if (!swi_x (0x06, r) && (r[1] & 255) == 255) esc = 1;
  if (alarm_on && ((now_cs () - alarm_at) & 0xFFFFFFFFUL) < 0x80000000UL) { alarm_on = 0; alrm = 1; }
  busy = 0;						/* before a handler runs: it may not return (longjmp) */
  if (esc) raise (SIGINT);
  if (alrm) raise (SIGALRM);
  if (__ul64_poll_timers) __ul64_poll_timers ();
}

int __ul64_take_signalled (void) { int s = signalled; signalled = 0; return s; }	/* nanosleep: did a handler run? */

unsigned int alarm (unsigned int seconds)
{
  unsigned int left = 0;
  if (alarm_on)
    {
      long d = (long) ((alarm_at - now_cs ()) & 0xFFFFFFFFUL);
      left = d > 0 && d < 0x40000000L ? (unsigned int) ((d + 99) / 100) : 0;
    }
  alarm_on = seconds != 0;
  alarm_at = now_cs () + (unsigned long) seconds * 100;
  return left;
}
int pause (void)
{
  signalled = 0;
  for (;;)						/* until a signal handler has run */
    {
      __ul64_poll_signals ();
      if (signalled) { signalled = 0; errno = EINTR; return -1; }
    }
}


int sigpending (sigset_t *set) { if (!set) { errno = EFAULT; return -1; } *set = pending; return 0; }

int sigsuspend (const sigset_t *mask)
{
  sigset_t old;
  int r;
  sigprocmask (SIG_SETMASK, mask, &old);		/* a signal that was pending runs here */
  r = pause ();
  sigprocmask (SIG_SETMASK, &old, 0);
  return r;
}

int sigwait (const sigset_t *set, int *sig)
{
  int s;
  for (;;)
    {
      __ul64_poll_signals ();
      for (s = 1; s < NSIG_; s++)
	if ((*set & BIT (s)) && (pending & BIT (s))) { pending &= ~BIT (s); *sig = s; return 0; }
    }
}

int sigaltstack (const stack_t *ss, stack_t *old)	/* a handler runs on the stack of the program: nothing to set up */
{
  (void) ss;
  if (old) { old->ss_sp = 0; old->ss_size = 0; old->ss_flags = SS_DISABLE; }
  return 0;
}

/* ITIMER_REAL only, in centiseconds like alarm () */
static void itimer_left (struct itimerval *o)
{
  long d = alarm_on ? (long) ((alarm_at - now_cs ()) & 0xFFFFFFFFUL) : 0;
  if (d < 0 || d > 0x40000000L) d = 0;
  o->it_interval.tv_sec = o->it_interval.tv_usec = 0;
  o->it_value.tv_sec = d / 100; o->it_value.tv_usec = (d % 100) * 10000;
}

int setitimer (int which, const struct itimerval *__restrict n, struct itimerval *__restrict o)
{
  if (which != ITIMER_REAL) { errno = EINVAL; return -1; }
  if (o) itimer_left (o);
  alarm_on = n->it_value.tv_sec != 0 || n->it_value.tv_usec != 0;
  alarm_at = now_cs () + (unsigned long) n->it_value.tv_sec * 100 + (unsigned long) (n->it_value.tv_usec / 10000);
  return 0;
}

int getitimer (int which, struct itimerval *o)
{
  if (which != ITIMER_REAL) { errno = EINVAL; return -1; }
  itimer_left (o);
  return 0;
}
