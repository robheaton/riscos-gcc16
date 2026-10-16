/* cxxheap64.c - malloc, free, calloc, realloc and abort for a C++ module that does not link the kit's C library (modkit/lib/heap.c, exit.c have the full ones): blocks of the RMA (OS_Module 6 and 7), 16 byte
   aligned, with a 16 byte header.  This object is in libmodcxx64.a, which is searched after the library: the library's own malloc is used if the module has it. */
#include "modkit64.h"
#pragma GCC visibility push(hidden)
void *malloc (size_t n)
{
  uint64_t r[10] = { 6, 0, 0, n + 32 };
  uintptr_t raw, p;
  if (n > 0x7FFFFF00u || mk_swi (0x1E, r)) return 0;
  raw = r[2];
  p = (raw + 16 + 15) & ~(uintptr_t) 15;
  ((uint32_t *) p)[-1] = (uint32_t) raw;
  ((uint32_t *) p)[-2] = (uint32_t) n;
  return (void *) p;
}
void free (void *p)
{
  uint64_t r[10];
  if (!p) return;
  r[0] = 7; r[1] = 0; r[2] = ((uint32_t *) p)[-1];
  mk_swi (0x1E, r);
}
void *calloc (size_t n, size_t size)
{
  size_t total = n * size, i;
  char *p;
  if (size && total / size != n) return 0;
  p = malloc (total);
  if (p) for (i = 0; i < total; i++) p[i] = 0;
  return p;
}
void *realloc (void *p, size_t n)
{
  size_t old;
  void *q;
  if (!p) return malloc (n);
  if (!n) { free (p); return 0; }
  old = ((uint32_t *) p)[-2];
  if (n <= old) return p;
  q = malloc (n);
  if (!q) return 0;
  memcpy (q, p, old);
  free (p);
  return q;
}
void abort (void)
{
  static struct { uint32_t num; char msg[40]; } err = { 0x1B0, "abort () was called in a module" };
  register uint64_t x0 __asm__ ("x0") = (uint64_t) (uintptr_t) &err, x10 __asm__ ("x10") = 0x2B;
  __asm__ volatile ("svc\t#0" : "+r" (x0) : "r" (x10) : "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "memory", "cc");	/* OS_GenerateError (no X bit: the error handler of the caller takes it, as for exit () in modkit/lib/exit.c) */
  for (;;) ;
}
