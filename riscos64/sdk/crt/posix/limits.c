/* limits.c - getrlimit and setrlimit: the limits of the process are not enforced (RISC OS has no such thing); the stack is what the application space leaves.  The SDK of RISC OS 64. */
#include <errno.h>
#include <sys/resource.h>

extern unsigned long __riscos_ramlimit;
extern char __heap_start[];

int getrlimit (int resource, struct rlimit *rl)
{
  if (!rl) { errno = EFAULT; return -1; }
  rl->rlim_cur = rl->rlim_max = RLIM_INFINITY;
  if (resource == RLIMIT_STACK) rl->rlim_cur = rl->rlim_max = (rlim_t) (__riscos_ramlimit - (unsigned long) __heap_start) / 2;
  return 0;
}

int setrlimit (int resource, const struct rlimit *rl)
{
  (void) resource; (void) rl;
  return 0;
}
