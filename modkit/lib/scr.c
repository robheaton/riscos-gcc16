/* scr.c - one character to the screen: OS_WriteC, and OS_NewLine for a line feed (the console needs LF CR).  printf, puts and putchar use it, and so do the streams stdout and stderr (fwrite.c). */
#include <stddef.h>

int (*__modlib_stdout_hook) (int);                                                               /* set by fcore.c while stdout is not the screen (freopen, fclose): printf, puts and putchar then write with it */

#ifdef MODLIB_HOST
extern void *__modlib_xswi (unsigned swi, unsigned *regs);                                       /* the host model of the SWIs (tests/libtest/hosthooks.c) */
void __modlib_scrputc (int c)
{
  unsigned r[10] = { (unsigned) c & 255 };
  __modlib_xswi (c == '\n' ? 0x20003u : 0x20000u, r);
}
#else
void __modlib_scrputc (int c)
{
  if (c == '\n')
    __asm__ volatile ("swi\t0x20003" : : : "lr", "memory", "cc");                               /* XOS_NewLine */
  else
    {
      register int r0 __asm__ ("r0") = (unsigned char) c;
      __asm__ volatile ("swi\t0x20000" : "+r" (r0) : : "lr", "memory", "cc");                    /* XOS_WriteC */
    }
}
#endif
