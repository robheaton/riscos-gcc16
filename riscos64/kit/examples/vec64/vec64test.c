/* vec64test - claims WrchV from Vec64, writes 10 characters with OS_WriteC and says how many the veneer saw. */
#include <stdio.h>
extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);
int main (void)
{
  unsigned long r[10] = { 0 }; unsigned long e; int i, fails = 0;
  printf ("RISC OS 64 test vec64 (a vector claim from a module)\n");
  { int mode; for (mode = 0; mode < 3; mode++) { r[0] = (unsigned long) mode; e = swi_x (0x59C80, r); printf ("INFO claim mode %d: %s\n", mode, e ? (const char *) ((const unsigned *) r[0] + 1) : "ok"); if (!e) break; } }
  if (e) { const unsigned *eb = (const unsigned *) r[0]; printf ("INFO claim error &%X: %s\n", eb[0], (const char *) (eb + 1)); } else printf ("INFO claim ok\n"); if (e) { printf ("FAIL claim\nSUMMARY 1 failed\n"); return 1; }
  r[0] = 0; swi_x (0x59C82, r); unsigned long before = r[0];
  fflush (stdout);
  for (i = 0; i < 10; i++) { unsigned long w[10] = { '.' }; swi_x (0x00, w); }
  swi_x (0x0A, (unsigned long[10]) { 0 });
  r[0] = 0; swi_x (0x59C82, r);
  printf ("\nINFO the veneer saw %lu calls (before %lu)\n", r[0] - before, before);
  if (r[0] - before < 10) { fails++; printf ("FAIL the veneer saw fewer than 10 calls\n"); } else printf ("PASS the veneer saw the calls\n");
  e = swi_x (0x59C81, r); if (e) { fails++; printf ("FAIL release\n"); } else printf ("PASS release\n");
  r[0] = 0; swi_x (0x59C82, r); before = r[0];
  for (i = 0; i < 5; i++) { unsigned long w[10] = { '.' }; swi_x (0x00, w); }
  r[0] = 0; swi_x (0x59C82, r);
  if (r[0] != before) { fails++; printf ("FAIL calls after release (%lu)\n", r[0] - before); } else printf ("PASS no calls after release\n");
  printf ("SUMMARY %d failed\n", fails);
  return fails != 0;
}
