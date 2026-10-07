/* swis.h - _swi () and _swix () and the macros of their register mask, as in the Acorn / SharedCLibrary header.  The SWI NUMBERS of the RISC OS Open sources (OS_WriteC, Wimp_Initialise, ... and the X versions) are in swisnums.h, which this includes, as the SharedCLibrary's swis.h has them; for the SWIs of modules that are not part of the OS use OSLib's headers (oslib/os.h ...) or your own constants.
     _swix (SWI_number, _INR (0, 2) | _OUT (1), r0, r1, r2, &out1)   returns NULL or the error block; the outputs are stored only when there is no error
     _swi (SWI_number, ...)                                          returns the register of _RETURN (n) (r0 when there is none; _RETURN (_FLAGS): the flags); an error is not returned (unless the number
                                                                     has the X bit: then the error block comes back as the result)
   The arguments are, in this order: the input registers (r0 - r9), pointers to the outputs (r0 - r9), a pointer for the flags (_OUT (_FLAGS)), then the words of a _BLOCK. */
#ifndef __SWIS_H
#define __SWIS_H
#include <stdarg.h>
#include <kernel.h>
#include <swisnums.h>
#define _FLAGS		0x10
#define _IN(i)		(1U << (i))
#define _INR(i, j)	(~0U << (i) ^ ~0U << ((j) + 1))
#define _OUT(i)		((i) != _FLAGS ? (1U << (31 - (i))) : 1U << 21)
#define _OUTR(i, j)	(~0U >> (i) ^ ~0U >> ((j) + 1))
#define _BLOCK(i)	(1U << 11 | (unsigned) (i) << 12)
#define _RETURN(i)	((i) != _FLAGS ? (unsigned) (i) << 16 : 0xFU << 16)
#define _C		(1U << 29)
#define _Z		(1U << 30)
#define _N		(1U << 31)
#define _V		(1U << 28)
#define XOS_Bit		(1U << 17)				/* (deprecated, as in the SharedCLibrary: use _swix) */
extern int _swi (int swi_no, unsigned int mask, ...);
extern _kernel_oserror *_swix (int swi_no, unsigned int mask, ...);
extern int _vswi (int swi_no, unsigned int mask, va_list ap);						/* the same with a va_list */
extern _kernel_oserror *_vswix (int swi_no, unsigned int mask, va_list ap);
#endif
