/* svc64.c - the service entry of the kit, tested from inside: sv_service claims 0x59BE1 (r1 = 0) and only counts 0x59BE2.  Svc64_Check calls __modrt_service_any, the C part of the entry, the way RISC OS Pyromaniac
   does (x1 = the number, the registers as the caller had them) and the way a kernel with 64-bit slots does (x0 = the number, x1 = the address of a block of 64-bit slots, a claim is a 0 in r1 of the block), and says in
   r0 which of 8 checks held.   aarch64-riscos-gcc -mmodule -O2 svc64.c svc64_hdr.o -o Svc64,ffa */
#include <stdint.h>
#include "kernel.h"
#include "modkit64.h"
#include "svc64h.h"

extern void __modrt_service_any (uint64_t x0, uint64_t x1, uint64_t *x, void *pw);
extern int __mod_slot;
static int seen;

void sv_service (int service, _kernel_swi_regs *r, void *pw)
{
  (void) pw;
  seen++;
  if (service == 0x59BE1) r->r[1] = 0;				/* claim */
}

_kernel_oserror *sv_swi (int offset, _kernel_swi_regs *r, void *pw)
{
  uint64_t block[10] __attribute__ ((aligned (16)));
  uint64_t x[10];
  unsigned ok = 0;
  int i, before, slot = __mod_slot;
  if (offset != 0) return error_BAD_SWI;
  for (i = 0; i < 10; i++) block[i] = x[i] = 0xC0 + i;

  /* a kernel with 64-bit slots: x0 = the number, x1 = the address of the block; a claim is a 0 in r1 of the block */
  block[0] = 0xA0; block[1] = 0x59BE1; before = seen;
  __modrt_service_any (0x59BE1, (uint64_t) (uintptr_t) block, x, pw);
  if (seen == before + 1 && block[1] == 0) ok |= 1u << 0;			/* called, and claimed in the block */
  block[1] = 0x59BE2; before = seen;
  __modrt_service_any (0x59BE2, (uint64_t) (uintptr_t) block, x, pw);
  if (seen == before + 1 && block[1] == 0x59BE2 && block[2] == 0xC2) ok |= 1u << 1;	/* called, not claimed, the other slots untouched */
  block[1] = 0x59BE3; before = seen;
  __modrt_service_any (0x59BE3, (uint64_t) (uintptr_t) block, x, pw);
  if (seen == before && block[1] == 0x59BE3) ok |= 1u << 2;			/* a number that is not in the list: not called */
  if (__mod_slot == 8) ok |= 1u << 3;						/* the call found that the slots are 64 bits */
  __mod_slot = slot;

  /* Pyromaniac: x1 = the number, the registers as the caller gave them, in the array; a claim is a 0 in x1 */
  x[0] = 0xA0; x[1] = 0x59BE1; before = seen;
  __modrt_service_any (x[0], x[1], x, pw);
  if (seen == before + 1 && x[1] == 0) ok |= 1u << 4;
  x[1] = 0x59BE2; x[2] = 0xC2; before = seen;
  __modrt_service_any (x[0], x[1], x, pw);
  if (seen == before + 1 && x[1] == 0x59BE2 && x[2] == 0xC2) ok |= 1u << 5;
  x[1] = 0x59BE3; before = seen;
  __modrt_service_any (x[0], x[1], x, pw);
  if (seen == before) ok |= 1u << 6;
  if (__mod_slot == slot) ok |= 1u << 7;
  r->r[0] = ok;
  r->r[1] = seen;
  return 0;
}
