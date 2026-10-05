/* host-stubs/kernel.h -- just enough of UnixLib's <kernel.h> for the host test of sultrace.h (host-sultrace-test.c) */
#ifndef HOST_KERNEL_H
#define HOST_KERNEL_H
typedef struct { int errnum; char errmess[252]; } _kernel_oserror;
typedef struct { int r[10]; } _kernel_swi_regs;
_kernel_oserror *_kernel_swi_c (int no, _kernel_swi_regs *in, _kernel_swi_regs *out, int *carry);
#endif
