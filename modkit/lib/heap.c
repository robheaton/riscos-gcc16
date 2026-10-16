/* heap.c - malloc, free, calloc, realloc: memory from the RMA (OS_Module 6 and 7).  Every block is a block of OS_Module 6; an 8 byte header gives the pointer that OS_Module 7 needs, the payload is
   8 byte aligned. */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#define XOS_MODULE   0x2001E

#ifdef __aarch64__
/* AArch64: the SWIs go through __modlib_xswi (the 32-bit register block; the addresses of the RMA are below 4 GB), the payload is 16 byte aligned and the header is 16 bytes */
extern void *__modlib_xswi (unsigned swi_x, unsigned *regs);
#define HDR 16
static void *alloc_aligned (size_t n, size_t align)
{
  unsigned r[10] = { 6, 0, 0, 0 };
  size_t extra = align > 16 ? align : 0, a = align > 16 ? align : 16;
  uintptr_t raw, payload;
  if (n > 0x7FFFFFF0u - extra) { errno = ENOMEM; return 0; }
  r[3] = (unsigned) (n + 2 * HDR + extra);
  if (__modlib_xswi (XOS_MODULE | 0x20000, r)) { errno = ENOMEM; return 0; }
  raw = r[2];
  payload = (raw + HDR + a - 1) & ~(uintptr_t) (a - 1);
  ((unsigned *) payload)[-1] = (unsigned) raw;
  ((unsigned *) payload)[-2] = (unsigned) n;
  return (void *) payload;
}
static void rma_free (void *p)
{
  unsigned r[10] = { 7, 0, ((unsigned *) p)[-1] };
  __modlib_xswi (XOS_MODULE | 0x20000, r);
}
#else
static void *alloc_aligned (size_t n, size_t align)
{
  unsigned raw; int err;
  size_t extra = align > 8 ? align : 0;
  if (n > 0x7FFFFFF0u - extra) { errno = ENOMEM; return 0; }                    /* (n + 16 + extra must not wrap round: the RMA could never give that much anyway) */
  register int r0 __asm__ ("r0") = 6;
  register unsigned r3 __asm__ ("r3") = (unsigned) (n + 16 + extra);
  register unsigned r2 __asm__ ("r2") = 0;
  __asm__ volatile ("swi\t%[swi]\n\t"
		    "movvs\t%[err], #1\n\t"
		    "movvc\t%[err], #0\n\t"
		    "mov\t%[raw], r2\n\t"
		    : [err] "=&r" (err), [raw] "=&r" (raw), "+r" (r0), "+r" (r2), "+r" (r3)
		    : [swi] "i" (XOS_MODULE) : "lr", "memory", "cc");
  if (err) { errno = ENOMEM; return 0; }
  size_t a = align > 8 ? align : 8;
  unsigned payload = (unsigned) ((raw + 8 + a - 1) & ~(a - 1));
  ((unsigned *) payload)[-1] = raw;
  ((unsigned *) payload)[-2] = (unsigned) n;
  return (void *) payload;
}
static void rma_free (void *p)
{
  register int r0 __asm__ ("r0") = 7;
  register unsigned r2 __asm__ ("r2") = ((unsigned *) p)[-1];
  __asm__ volatile ("swi\t%[swi]" : "+r" (r0), "+r" (r2) : [swi] "i" (XOS_MODULE) : "lr", "memory", "cc");
}
#endif
void *malloc (size_t n) { return alloc_aligned (n, 8); }
void *aligned_alloc (size_t alignment, size_t n)
{
  if (!alignment || (alignment & (alignment - 1)) || alignment > 0x10000000u) { errno = EINVAL; return 0; }
  return alloc_aligned (n, alignment);
}
void free (void *p)
{
  if (!p) return;
  rma_free (p);
}
void *calloc (size_t n, size_t size)
{
  size_t total = n * size;
  if (size && total / size != n) { errno = ENOMEM; return 0; }
  void *p = malloc (total);
  if (p) memset (p, 0, total);
  return p;
}
void *realloc (void *p, size_t n)
{
  if (!p) return malloc (n);
  if (!n) { free (p); return 0; }
  size_t old = ((unsigned *) p)[-2];
  if (n <= old) return p;
  void *q = malloc (n);
  if (!q) return 0;
  memcpy (q, p, old);
  free (p);
  return q;
}
