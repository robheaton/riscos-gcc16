/* Host mock of UnixLib's <kernel.h>: just what riscos-throwback.cc uses.  */
#ifndef MOCK_KERNEL_H
#define MOCK_KERNEL_H
typedef struct { int r[10]; } _kernel_swi_regs;
typedef struct { int errnum; char errmess[252]; } _kernel_oserror;
extern _kernel_oserror *_kernel_swi (int no, const _kernel_swi_regs *in, _kernel_swi_regs *out);
#endif
