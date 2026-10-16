/* platform64.c - what the kit's C library (modkit/lib, libmodkit64.a) needs of the machine on AArch64: the SWI calls with the library's block of ten 32-bit registers (the addresses of a module and of the RMA are
   below 4 GB, so a pointer in a register is the address itself), the clock, and OS_GenerateError.  The library is compiled with -DMODLIB_HOST (as for the host tests of libtest), which makes it call these
   instead of using the ARM inline assembler of the 32-bit build. */
#include <stddef.h>
#include <stdint.h>

typedef struct { int errnum; char errmess[252]; } oserr;
extern oserr *a64_swi (unsigned swi, unsigned *r, unsigned *flags);		/* modsys64.S */

unsigned mk_ptr_in (const void *p) { return (unsigned) (uintptr_t) p; }
oserr *__modlib_xswif (unsigned swi, unsigned *r, unsigned *flags)
{
  unsigned f = 0;
  oserr *e = a64_swi (swi, r, &f);
  if (flags) *flags = f;
  return e;
}
oserr *__modlib_xswi (unsigned swi, unsigned *r) { return __modlib_xswif (swi, r, 0); }
void __modlib_raise (const void *e)						/* OS_GenerateError (no X bit): the error goes to the error handler of the caller */
{
  unsigned r[10] = { (unsigned) (uintptr_t) e };
  a64_swi (0x2B, r, 0);
}
int mk_host_read_clock (unsigned char *b)					/* OS_Word 14, 3: the real time clock, 5 bytes of centiseconds since 1900 (time.c) */
{
  unsigned r[10] = { 14, (unsigned) (uintptr_t) b };
  b[0] = 3;
  return a64_swi (0x20007, r, 0) != 0;
}
