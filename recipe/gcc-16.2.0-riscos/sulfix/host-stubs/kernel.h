/* host-stubs/kernel.h -- just enough of UnixLib's <kernel.h> to build sulfile.c on the build host (host-main.c) */
#ifndef HOST_KERNEL_H
#define HOST_KERNEL_H
typedef struct { int errnum; char errmess[252]; } _kernel_oserror;
#endif
