/* host-stubs/swis.h -- the SWIs that sulfile.c uses and the register masks of _swix, for the model of host-main.c.  The masks are ours (inputs in bits 0-9, outputs in bits 16-25); the real
   <swis.h> has its own encoding, and the model only needs the same meaning: which registers are passed in, and which are returned through pointers, both in register order. */
#ifndef HOST_SWIS_H
#define HOST_SWIS_H
#include "kernel.h"
#define OS_File 0x08
#define OS_FSControl 0x29
#define OS_ReadVarVal 0x23
#define OS_SetVarVal 0x24
#define _IN(n) (1u << (n))
#define _INR(a, b) (((2u << (b)) - 1u) & ~((1u << (a)) - 1u))
#define _OUT(n) (0x10000u << (n))
#define _OUTR(a, b) ((((2u << (b)) - 1u) & ~((1u << (a)) - 1u)) << 16)
_kernel_oserror *_swix (int swi, unsigned flags, ...);
#endif
