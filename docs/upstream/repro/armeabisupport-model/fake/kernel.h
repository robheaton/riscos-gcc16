/* fake kernel.h for the host model of ARMEABISupport: only the types the module's allocator code uses */
#ifndef FAKE_KERNEL_H
#define FAKE_KERNEL_H
#include <stddef.h>
typedef struct { int errnum; char errmess[252]; } _kernel_oserror;
typedef struct { int r[10]; } _kernel_swi_regs;
#define XOS_Bit 0x20000
#endif
