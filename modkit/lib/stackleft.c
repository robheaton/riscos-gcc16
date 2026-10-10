/* stackleft.c - how much of the kernel's SVC stack is left.  The C code of a module that is entered through its header (initialisation, commands, SWIs, service calls) runs on the SVC stack: 32 KB
   that the whole OS shares (the kernel's SVCStackSize).  There is no overflow check (GCC for the ARM has none that RISC OS could use) and the stack cannot be moved: the Task window saves the SVC stack
   between its top and r13 whenever a task writes a character or sleeps, so an r13 anywhere else aborts it.  __modlib_stack_left tells a module that recurses or has big locals how far it can go. */
#include "kernel.h"

#define SVC_STACK_SIZE (32 * 1024)

extern _kernel_oserror *__modlib_xswi (unsigned swi_x, unsigned *regs);

/* The bytes between the stack pointer and the bottom of the SVC stack, or -1 when they cannot be known: the stack pointer is not on the SVC stack (the code runs in user mode, a runnable module
   uses its application stack), or OS_ReadSysInfo 6 does not give the top of the SVC stack. */
long __modlib_stack_left (void)
{
  static unsigned top;                                    /* the top of the SVC stack: it does not change while the machine is on */
  unsigned sp = (unsigned) __builtin_frame_address (0);
  if (!top)
    {
      unsigned list[2] = { 16, (unsigned) -1 }, out[2] = { 0, 0 }, r[10] = { 6, (unsigned) list, (unsigned) out };
      if (__modlib_xswi (0x20058, r)) return -1;          /* XOS_ReadSysInfo 6: the values of OSRSI6_SVCSTK (16) */
      top = out[0];
    }
  if (!top || sp > top || sp < top - SVC_STACK_SIZE) return -1;
  return (long) (sp - (top - SVC_STACK_SIZE));
}
