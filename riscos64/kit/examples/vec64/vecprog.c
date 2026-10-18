#include <stdio.h>
extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);
__attribute__ ((naked)) void veneer (void) { __asm__ volatile ("ret"); }
int main (void)
{
  unsigned long r[10] = { 3, (unsigned long) veneer, 0 };
  unsigned long e = swi_x (0x1F, r);
  printf ("claim from a program: %s\n", e ? (const char *) ((const unsigned *) r[0] + 1) : "ok");
  if (!e) { r[0] = 3; r[1] = (unsigned long) veneer; r[2] = 0; e = swi_x (0x20, r); printf ("release: %s\n", e ? "error" : "ok"); }
  return 0;
}
