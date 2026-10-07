/* heap.c - malloc, free, calloc, realloc: memory from the RMA (OS_Module 6 and 7).  Every block is a block of OS_Module 6; an 8 byte header gives the pointer that OS_Module 7 needs, the payload is
   8 byte aligned. */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define XOS_MODULE   0x2001E

void *malloc (size_t n)
{
  unsigned raw; int err;
  if (n > 0x7FFFFFF0u) { errno = ENOMEM; return 0; }                    /* (n + 16 must not wrap round: the RMA could never give that much anyway) */
  register int r0 __asm__ ("r0") = 6;
  register unsigned r3 __asm__ ("r3") = (unsigned) n + 16;
  register unsigned r2 __asm__ ("r2") = 0;
  __asm__ volatile ("swi\t%[swi]\n\t"
		    "movvs\t%[err], #1\n\t"
		    "movvc\t%[err], #0\n\t"
		    "mov\t%[raw], r2\n\t"
		    : [err] "=&r" (err), [raw] "=&r" (raw), "+r" (r0), "+r" (r2), "+r" (r3)
		    : [swi] "i" (XOS_MODULE) : "lr", "memory", "cc");
  if (err) { errno = ENOMEM; return 0; }
  unsigned payload = (raw + 8 + 7) & ~7u;
  ((unsigned *) payload)[-1] = raw;
  ((unsigned *) payload)[-2] = (unsigned) n;
  return (void *) payload;
}
void free (void *p)
{
  if (!p) return;
  register int r0 __asm__ ("r0") = 7;
  register unsigned r2 __asm__ ("r2") = ((unsigned *) p)[-1];
  __asm__ volatile ("swi\t%[swi]" : "+r" (r0), "+r" (r2) : [swi] "i" (XOS_MODULE) : "lr", "memory", "cc");
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
