/* slot64test - tests that the kit has found out the size of a slot by the probe SWI, and that three registers go in and out in the right places. */
#include <stdio.h>
extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);
int main (void)
{
  unsigned long r[10] = { 0, 0, 41 };
  int bad = 0;
  printf ("RISC OS 64 test slot64test\n");
  if (swi_x (0x59E00, r)) { printf ("FAIL Slot64_Report\n"); return 1; }
  printf ("INFO slot size = %lu, probe state = %lu, r2 = %lu, the register block was at &%lX%08lX\n", r[0] & 0xFFFFFFFF, r[1] & 0xFFFFFFFF, r[2] & 0xFFFFFFFF, r[4] & 0xFFFFFFFF, r[3] & 0xFFFFFFFF);
  if ((r[1] & 0xFFFFFFFF) != 2) { printf ("FAIL the probe did not answer\n"); bad++; } else printf ("PASS the probe answered\n");
  if ((r[2] & 0xFFFFFFFF) != 42) { printf ("FAIL r2 in and out\n"); bad++; } else printf ("PASS r2 in and out\n");
  printf ("SUMMARY %d failed\n", bad);
  return bad != 0;
}
