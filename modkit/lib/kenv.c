/* kenv.c - _kernel_getenv, _kernel_setenv and getenv: the system variables of RISC OS (OS_ReadVarVal, OS_SetVarVal). */
#pragma GCC optimize ("Os")                       /* not a hot path: the smaller code is the better one in a module */
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <kernel.h>
#include <swis.h>

extern _kernel_oserror *__modlib_xswi (unsigned swi_x, unsigned *regs);
extern _kernel_oserror *__modlib_xswif (unsigned swi_x, unsigned *regs, unsigned *flags);
extern _kernel_oserror *__modlib_remember (const _kernel_oserror *e);

#define CARRY	0x20000000u
#define X	0x20000u
#define remember __modlib_remember

_kernel_oserror *_kernel_getenv (const char *name, char *buffer, unsigned size)
{
  unsigned r[10] = { (unsigned) name, (unsigned) buffer, size - 1, 0, 3 };
  if (!size) return 0;
  _kernel_oserror *e = __modlib_xswi (0x23 | X, r);                  /* OS_ReadVarVal, converted to a string (R4 = 3) */
  if (e) return remember (e);
  buffer[r[2]] = 0;
  return 0;
}
_kernel_oserror *_kernel_setenv (const char *name, const char *value)
{
  unsigned len = 0;
  if (value) while (value[len]) len++;
  unsigned r[10] = { (unsigned) name, (unsigned) value, value ? len : (unsigned) -1, 0, 0 };
  _kernel_oserror *e = __modlib_xswi (0x24 | X, r);                  /* OS_SetVarVal: a string; no value deletes the variable */
  return e ? remember (e) : 0;
}


char *getenv (const char *name)
{
  static char buf[256];
  return _kernel_getenv (name, buf, sizeof buf) ? 0 : buf;
}
