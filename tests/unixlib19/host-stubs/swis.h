/* host-stubs/swis.h -- just enough of UnixLib's <swis.h> for the host test of sultrace.h */
#ifndef HOST_SWIS_H
#define HOST_SWIS_H
#include "kernel.h"
#define OS_ReadMonotonicTime 0x42
#define OS_Find 0x0D
#define OS_Args 0x09
#define OS_GBPB 0x0C
#define OS_File 0x08
#define _INR(a, b) 0
#define _OUT(n) 0
#define _OUTR(a, b) 0
_kernel_oserror *_swix (int swi, unsigned flags, ...);
#endif
