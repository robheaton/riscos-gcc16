/* control.c - __get_riscosify_control and __set_riscosify_control (libunixlib/common/_riscosify.s of UnixLib, in C): the flags that tell __riscosify what to do.  A program can define its own
   __riscosify_control (a weak symbol here); then that variable is used.  Revised BSD licence (licenses/UnixLib-COPYING.txt).  */
#include "ul64.h"

extern int __riscosify_control __attribute__ ((weak));
static int __riscosify_control_internal;

int __get_riscosify_control (void)
{
  return &__riscosify_control ? __riscosify_control : __riscosify_control_internal;
}

void __set_riscosify_control (int flags)
{
  if (&__riscosify_control) __riscosify_control = flags;
  else __riscosify_control_internal = flags;
}
