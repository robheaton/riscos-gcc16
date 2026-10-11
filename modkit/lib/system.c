/* system.c - system (): the command goes to the command line interpreter (OS_CLI, the X form: an error comes back as -1 with errno EIO, and the OS error stays available as _kernel_last_oserror ()).  system (NULL) says
   that there is a command processor (1). */
#include <errno.h>
#include <stdlib.h>
#include "kernel.h"

extern _kernel_oserror *__modlib_xswi (unsigned swi_x, unsigned *regs);

int system (const char *cmd)
{
  unsigned r[10] = { 0 };
  _kernel_oserror *e;
  if (!cmd) return 1;
  r[0] = (unsigned) cmd;
  e = __modlib_xswi (0x20005, r);                    /* XOS_CLI */
  if (e) { errno = EIO; return -1; }
  return 0;
}
