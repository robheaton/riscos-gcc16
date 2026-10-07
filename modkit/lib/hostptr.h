/* hostptr.h - a pointer in the registers of a SWI.  On the machine it is the address itself.  On the host (the library tests, MODLIB_HOST) a pointer does not fit in the 32 bits of a register, so the host
   model of the SWIs (tests/libtest/hosthooks.c) hands out a token for it that its SWI code turns back into the pointer. */
#ifndef _HOSTPTR_H
#define _HOSTPTR_H
#ifdef MODLIB_HOST
extern unsigned mk_ptr_in (const void *p);
#define PIN(p) mk_ptr_in (p)
#else
#define PIN(p) ((unsigned) (p))
#endif
#endif
