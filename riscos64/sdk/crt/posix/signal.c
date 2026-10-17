/* signal.c - signal, raise, sigaction, kill (of the process itself), sigprocmask and alarm, in the way that needs no operating system support: a table of handlers, raise () calls the handler of the signal, the default of
   a signal is to end the process (or nothing, for SIGCHLD, SIGURG, SIGWINCH).  Nothing arrives asynchronously yet (Escape as SIGINT, SIGALRM from a timer, a data abort as SIGSEGV: the second release of UnixLib 64).
   libriscosstub.a is searched before libc.a, so these take the place of picolibc's fallbacks.  The SDK of RISC OS 64. */
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#define NSIG_ 65
static void (*handlers[NSIG_]) (int);
static sigset_t blocked;

extern void _exit (int);

static int ignored_by_default (int sig) { return sig == SIGCHLD || sig == SIGURG || sig == SIGWINCH || sig == SIGCONT; }

void (*signal (int sig, void (*handler) (int))) (int)
{
  void (*old) (int);
  if (sig <= 0 || sig >= NSIG_ || sig == SIGKILL || sig == SIGSTOP) { errno = EINVAL; return SIG_ERR; }
  old = handlers[sig];
  handlers[sig] = handler;
  return old;
}

int raise (int sig)
{
  void (*h) (int);
  if (sig <= 0 || sig >= NSIG_) { errno = EINVAL; return -1; }
  h = handlers[sig];
  if (h == SIG_IGN) return 0;
  if (h == SIG_DFL || h == 0)
    {
      if (ignored_by_default (sig)) return 0;
      _exit (128 + sig);
    }
  handlers[sig] = SIG_DFL;							/* ISO C: the default action is restored before the handler runs (unless SA_RESETHAND is not wanted: see sigaction) */
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
  if (act) handlers[sig] = act->sa_handler;
  return 0;
}

int sigprocmask (int how, const sigset_t *set, sigset_t *old)
{
  if (old) *old = blocked;
  if (set)
    {
      if (how == SIG_BLOCK) blocked |= *set;
      else if (how == SIG_UNBLOCK) blocked &= ~*set;
      else blocked = *set;
    }
  return 0;
}

unsigned int alarm (unsigned int seconds) { (void) seconds; return 0; }
int pause (void) { errno = EINTR; return -1; }
