/* krma.c - _kernel_RMAalloc, _kernel_RMAextend and _kernel_RMAfree: blocks of the RMA straight from OS_Module (6 claim, 13 extend, 7 free), as the SharedCLibrary's kernel interface has them.  They keep no
   header of their own (malloc () keeps 8 bytes): a block made by one of these is freed by _kernel_RMAfree only. */
#include <stddef.h>
#include "kernel.h"

extern _kernel_oserror *__modlib_xswi (unsigned swi_x, unsigned *regs);
#define XOS_MODULE (0x1Eu | 0x20000u)

void *_kernel_RMAalloc (size_t size)
{
  unsigned r[10] = { 6, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  if (size == 0 || size > 0x7FFFFFF0u) return NULL;
  r[3] = (unsigned) size;
  if (__modlib_xswi (XOS_MODULE, r)) return NULL;
  return (void *) r[2];
}

void _kernel_RMAfree (void *p)
{
  unsigned r[10] = { 7, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  if (!p) return;
  r[2] = (unsigned) p;
  __modlib_xswi (XOS_MODULE, r);
}

void *_kernel_RMAextend (void *p, size_t size)
{
  unsigned r[10] = { 13, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  unsigned have;
  if (!p) return _kernel_RMAalloc (size);
  if (size == 0) { _kernel_RMAfree (p); return NULL; }
  if (size > 0x7FFFFFF0u) return NULL;
  have = ((unsigned *) p)[-1] - 4;                                      /* the RMA heap keeps the size of the block (with the word itself) in front of it */
  r[2] = (unsigned) p;
  r[3] = (unsigned) size - have;                                        /* the change of the size, negative to shrink */
  if (__modlib_xswi (XOS_MODULE, r)) return NULL;
  return (void *) r[2];
}
