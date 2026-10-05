/* dautil.h -- read-only helpers for the tests of libunixlib 16.2.0-6: the OS's clamps on the maximum size of dynamic areas, and a look at the dynamic areas (by name).
   Only SWIs that read are used (OS_DynamicArea 8 with R1 = R2 = 0 "read only", reasons 2 and 3).  A guarded read of the area name (the pointer comes from the kernel). */
#ifndef DAUTIL_H
#define DAUTIL_H
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <swis.h>

/* The clamps on the maximum size of non-sparse areas created with OS_DynamicArea 0: c1 for R5 = -1, c2 for R5 > 0 (the kernel reduces the size asked for to the clamp, without an
   error).  Returns 0 on success.  A clamp of -1 (or one above the RAM limit) means "the RAM limit of the machine". */
static __attribute__((unused)) int da_clamps(unsigned *c1, unsigned *c2)
{
  *c1 = *c2 = 0;
  return _swix(OS_DynamicArea, _INR(0, 2) | _OUTR(1, 2), 8, 0, 0, c1, c2) != NULL;
}

struct da_info { int number; unsigned size, base, flags, max, emax; char name[40]; };   /* max = OS_DynamicArea 2 R5 (as asked at creation), emax = OS_ReadDynamicArea R2 (the real one) */

static sigjmp_buf da_jb;
static void da_segv(int sig) { (void) sig; siglongjmp(da_jb, 1); }

/* fill I for area N; the name is read with a SIGSEGV guard ("?" when unreadable) */
static __attribute__((unused)) int da_get(int n, struct da_info *i)
{
  unsigned h = 0, ws = 0; const char *nm = NULL;
  i->number = n; i->name[0] = 0;
  if (_swix(OS_DynamicArea, _INR(0, 1) | _OUTR(2, 8), 2, n, &i->size, &i->base, &i->flags, &i->max, &h, &ws, &nm)) return 1;
  { unsigned b = 0, sz = 0, em = 0; i->emax = _swix(OS_ReadDynamicArea, _IN(0) | _OUTR(0, 2), n, &b, &sz, &em) ? 0 : em; }
  struct sigaction sa, old; memset(&sa, 0, sizeof sa); sa.sa_handler = da_segv; sigaction(SIGSEGV, &sa, &old);
  if (sigsetjmp(da_jb, 1) == 0) { strncpy(i->name, nm ? nm : "?", sizeof i->name - 1); i->name[sizeof i->name - 1] = 0; }
  else strcpy(i->name, "?");
  sigaction(SIGSEGV, &old, NULL);
  return 0;
}

/* the next area number after N (-1 to start); -1 at the end */
static __attribute__((unused)) int da_next(int n)
{
  int r = -1;
  if (_swix(OS_DynamicArea, _INR(0, 1) | _OUT(1), 3, n, &r)) return -1;
  return r;
}

/* find the area called NAME; returns 0 and fills I, or 1 */
static __attribute__((unused)) int da_find(const char *name, struct da_info *i)
{
  for (int n = da_next(-1), guard = 0; n != -1 && guard < 500; n = da_next(n), guard++) {
    struct da_info t;
    if (!da_get(n, &t) && !strcmp(t.name, name)) { *i = t; return 0; }
  }
  return 1;
}

/* The LOGICAL window of an area that was created with OS_DynamicArea 0: what it asked for, cut down to the OS clamp (OS_DynamicArea 8) when one is set.  (For a page-mapped area, like ARMEABISupport's
   "UnixLib stacks", both SWIs report the PHYSICAL maximum it asked for, 256 MB, while the kernel only gave it a window of the clamp's size.) */
static __attribute__((unused)) unsigned long long da_window(const struct da_info *i)
{
  unsigned long long w = i->emax ? i->emax : i->max;
  unsigned c1, c2;
  if (da_clamps(&c1, &c2) == 0 && (int) c2 != -1 && c2 != 0 && c2 < w) w = c2;
  return w;
}
#endif
