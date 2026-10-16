/* swi64test - runs Swi64_Check: the checks of _swi and _swix inside the module. */
#include <stdio.h>
extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);
int main (void)
{
  unsigned long r[10] = { 0 };
  int fails = 0, n;
  printf ("RISC OS 64 test swi64test (_swi and _swix in a module)\n");
  if (swi_x (0x59BC0, r)) { printf ("FAIL Swi64_Check gave an error\n"); return 1; }
  printf ("INFO checks = &%lX\n", r[0] & 0xFFFFFFFF);
  for (n = 0; n < 12; n++) { int ok = (r[0] >> n) & 1; if (!ok) fails++; printf ("%s check %d\n", ok ? "PASS" : "FAIL", n); }
  printf ("SUMMARY %d failed\n", fails);
  return fails != 0;
}
