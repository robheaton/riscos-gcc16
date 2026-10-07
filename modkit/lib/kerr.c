/* kerr.c - the last OS error that a _kernel_ call met (_kernel_last_oserror gives it once). */
#include <stddef.h>
#include <kernel.h>

static _kernel_oserror last_err;
static int last_valid;

_kernel_oserror *__modlib_remember (const _kernel_oserror *e)
{
  size_t i;
  last_err.errnum = e->errnum;
  for (i = 0; i < sizeof last_err.errmess - 1 && e->errmess[i]; i++) last_err.errmess[i] = e->errmess[i];
  last_err.errmess[i] = 0;
  last_valid = 1;
  return &last_err;
}
_kernel_oserror *_kernel_last_oserror (void)
{
  if (!last_valid) return 0;
  last_valid = 0;
  return &last_err;
}

