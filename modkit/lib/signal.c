/* signal.c - signal () and raise (): a table of handlers (see signal.h).  A handler is reset to SIG_DFL before it is called (ISO C). */
#include <signal.h>
#include <stdlib.h>
#include <errno.h>

static __sighandler_t handlers[_NSIG];

__sighandler_t signal (int sig, __sighandler_t func)
{
  __sighandler_t old;
  if (sig <= 0 || sig >= _NSIG || func == SIG_ERR) { errno = EINVAL; return SIG_ERR; }
  old = handlers[sig];
  handlers[sig] = func;
  return old;
}

int raise (int sig)
{
  __sighandler_t h;
  if (sig <= 0 || sig >= _NSIG) return -1;
  h = handlers[sig];
  if (h == SIG_IGN) return 0;
  if (h == SIG_DFL) abort ();
  handlers[sig] = SIG_DFL;
  h (sig);
  return 0;
}
