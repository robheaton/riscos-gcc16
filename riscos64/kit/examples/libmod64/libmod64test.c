/* libmod64test - runs Libmod64_Check (the checks of the C library inside the module) and the command. */
#include <stdio.h>
extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);
int main (void)
{
  unsigned long r[10] = { 0 };
  int fails = 0, n;
  printf ("RISC OS 64 test libmod64test (a module that uses the kit's C library)\n");
  if (swi_x (0x59B80, r)) { printf ("FAIL Libmod64_Check gave an error\n"); return 1; }
  printf ("INFO checks = &%lX\n", r[0] & 0xFFFFFFFF);
  for (n = 0; n < 12; n++) { int ok = (r[0] >> n) & 1; if (!ok) fails++; printf ("%s check %d\n", ok ? "PASS" : "FAIL", n); }
  { const char *c = "Libmod64_Show"; unsigned long q[10] = { (unsigned long) c }; if (swi_x (5, q)) { fails++; printf ("FAIL *Libmod64_Show\n"); } else printf ("PASS *Libmod64_Show\n"); }
  printf ("SUMMARY %d failed\n", fails);
  return fails != 0;
}
