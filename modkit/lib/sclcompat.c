/* sclcompat.c - run-time names of the Shared C Library and the Norcroft compiler that the C and assembler sources of the RISC OS modules use, with the meaning they have in a module made by this kit */

/* __current_sp (): the stack pointer of the caller (DOSFS checks that there is room on the stack) */
unsigned __current_sp (void) __attribute__ ((naked));
unsigned __current_sp (void)
{
  __asm__ volatile ("mov\tr0, sp\n\tbx\tlr");
}

/* _sprintf: the name of sprintf inside the Shared C Library, which the assembler sources of the OS (the Toolbox's wimplib) call */
#include <stdarg.h>
#include <stdio.h>
int _sprintf (char *s, const char *fmt, ...)
{
  va_list ap;
  int n;
  va_start (ap, fmt);
  n = vsprintf (s, fmt, ap);
  va_end (ap);
  return n;
}
