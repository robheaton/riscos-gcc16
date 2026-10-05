/* kernel.h - the part of SharedCLibrary's <kernel.h> that a module needs: the error block, the register block, _kernel_swi.  (modkit: modules are built without any C library.) */
#ifndef __KERNEL_H
#define __KERNEL_H
typedef struct { int errnum; char errmess[252]; } _kernel_oserror;
typedef struct { int r[10]; } _kernel_swi_regs;
#define _kernel_NONX 0x80000000
/* Call the SWI NO with the registers IN, return the registers in OUT.  The X bit is set unless bit 31 (_kernel_NONX) is set. */
extern _kernel_oserror *_kernel_swi (int no, const _kernel_swi_regs *in, _kernel_swi_regs *out);
extern _kernel_oserror *_kernel_swi_c (int no, const _kernel_swi_regs *in, _kernel_swi_regs *out, int *carry);
#endif
