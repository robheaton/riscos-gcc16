/* fpmod64test - tests Fpmod64 (floating point in a module) and checks that the caller's floating point registers are kept across the module's SWI. */
#include <stdio.h>
extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);
static int checks, fails;
#define CHECK(c, what) do { checks++; if (!(c)) { fails++; printf ("FAIL %s (line %d)\n", what, __LINE__); } else printf ("PASS %s\n", what); } while (0)
#define L(i) ((long) (int) (r[i] & 0xFFFFFFFF))
int main (void)
{
  unsigned long r[10] = { 7, 6, 8, 4 };
  double keep = 3.25;
  __asm__ volatile ("" : "+w" (keep));				/* in a d register across the SWI */
  printf ("RISC OS 64 test fpmod64test\n");
  CHECK (!swi_x (0x59B40, r), "Fpmod64_Calc");
  printf ("INFO %ld %ld %ld\n", L(0), L(1), L(2));
  CHECK (L(0) == 12500, "(7 * 6 + 8) / 4 = 12.5");
  CHECK (L(1) == 9219, "sqrt (7 * 7 + 6 * 6) = 9.219...");
  CHECK (L(2) == 7485470, "the harmonic sum of 1000 terms = 7.485470...");
  CHECK (keep == 3.25, "the caller's floating point register is kept");
  printf ("SUMMARY %d checks, %d failed\n", checks, fails);
  return fails != 0;
}
