/* strext.c - string functions that are not in the C standard: stpcpy, stpncpy, mempcpy, bcmp, ffs, ffsl, ffsll, strichr, strrichr, strsignal and the signal texts (sys_siglist). */
#include <stddef.h>
#include <string.h>
#include <signal.h>

char *stpcpy (char *d, const char *s)
{
  while ((*d = *s++) != 0) d++;
  return d;
}
char *stpncpy (char *d, const char *s, size_t n)
{
  size_t i = 0;
  while (i < n && s[i]) { d[i] = s[i]; i++; }
  char *end = d + i;
  while (i < n) d[i++] = 0;
  return end;
}
void *mempcpy (void *d, const void *s, size_t n) { return (char *) memcpy (d, s, n) + n; }
int bcmp (const void *a, const void *b, size_t n) { return memcmp (a, b, n); }
int ffs (int i)
{
  unsigned u = (unsigned) i;
  int n = 1;
  if (!u) return 0;
  while (!(u & 1)) { u >>= 1; n++; }
  return n;
}
int ffsl (long i) { return ffs ((int) i); }                                  /* (long is 32 bits) */
int ffsll (long long i)
{
  unsigned long long u = (unsigned long long) i;
  if (!u) return 0;
  return (unsigned) u ? ffs ((int) (unsigned) u) : 32 + ffs ((int) (unsigned) (u >> 32));
}
static int fold (int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
char *strichr (const char *s, int c)
{
  c = fold ((unsigned char) c);
  for (;; s++)
    {
      if (fold ((unsigned char) *s) == c) return (char *) s;
      if (!*s) return 0;
    }
}
char *strrichr (const char *s, int c)
{
  const char *last = 0;
  c = fold ((unsigned char) c);
  for (;; s++)
    {
      if (fold ((unsigned char) *s) == c) last = s;
      if (!*s) return (char *) last;
    }
}

int sys_nsig = _NSIG;
const char *const sys_siglist[_NSIG] =
{
  [0] = "Signal 0", [SIGABRT] = "Aborted", [SIGFPE] = "Floating point exception", [SIGILL] = "Illegal instruction", [SIGINT] = "Interrupt", [SIGSEGV] = "Segmentation fault",
  [SIGTERM] = "Terminated", [SIGSTAK] = "Stack overflow", [SIGUSR1] = "User defined signal 1", [SIGUSR2] = "User defined signal 2", [SIGOSERROR] = "Operating system error",
};
char *strsignal (int sig)
{
  static char buf[32];
  char num[12], *p = num + sizeof num;
  unsigned u;
  if (sig > 0 && sig < _NSIG) return (char *) sys_siglist[sig];
  u = sig < 0 ? -(unsigned) sig : (unsigned) sig;
  *--p = 0;
  do *--p = (char) ('0' + u % 10); while (u /= 10);
  if (sig < 0) *--p = '-';
  strcpy (buf, "Unknown signal ");
  strcat (buf, p);
  return buf;
}
