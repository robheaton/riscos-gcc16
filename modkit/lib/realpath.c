/* realpath.c - realpath (): the canonical RISC OS name of an object that exists (OS_FSControl 37, then OS_File 17 to see that it is there). */
#pragma GCC optimize ("Os")                       /* not a hot path: the smaller code is the better one in a module */
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <kernel.h>

extern _kernel_oserror *__modlib_xswi (unsigned swi_x, unsigned *regs);
#define X 0x20000u

char *realpath (const char *file, char *resolved)
{
  char buf[PATH_MAX];
  unsigned r[10];
  char *out = resolved;
  if (!file) { errno = EINVAL; return 0; }
  r[0] = 37; r[1] = (unsigned) file; r[2] = (unsigned) buf; r[3] = 0; r[4] = 0; r[5] = sizeof buf;
  if (__modlib_xswi (0x29 | X, r)) { errno = ENOENT; return 0; }                 /* OS_FSControl 37: canonicalise the name */
  if ((int) r[5] < 0) { errno = ENAMETOOLONG; return 0; }
  r[0] = 17; r[1] = (unsigned) buf;
  if (__modlib_xswi (0x08 | X, r) || r[0] == 0) { errno = ENOENT; return 0; }    /* OS_File 17: the type of the object (0 = nothing there) */
  if (!out) { out = malloc (PATH_MAX); if (!out) return 0; }
  strcpy (out, buf);
  return out;
}
