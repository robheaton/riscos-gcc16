/* exp_da - E6: can code run from a dynamic area?  The program makes an area, copies "mov x0, #42; ret" into it and calls it. If the call aborts, the output ends before "called". */
#include <stdio.h>
#include <string.h>
extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);
int main (void)
{
  unsigned long r[10] = { 0, (unsigned long) -1, 8192, (unsigned long) -1, 0, 1UL << 20, 0, 0, (unsigned long) "UL64 exec test" };
  static const unsigned code[2] = { 0xD2800540u /* mov x0, #42 */, 0xD65F03C0u /* ret */ };
  unsigned long base;
  long (*fn) (void);
  if (swi_x (0x66, r)) { printf ("OS_DynamicArea 0 failed: %s\n", (char *) r[0] + 4); return 1; }
  base = r[3];
  printf ("area %lu at %#lx\n", r[1] & 0xFFFFFFFF, base);
  memcpy ((void *) base, code, sizeof code);
  { unsigned long s[10] = { 1 << 0, base, base + sizeof code };		/* OS_SynchroniseCodeAreas: R0 bit 0 = a range */
    printf ("synchronise: %s\n", swi_x (0x6E, s) ? "error" : "ok"); }
  fn = (long (*) (void)) base;
  printf ("calling %#lx\n", base);
  fflush (stdout);
  printf ("called: %ld\n", fn ());
  return 1;
}
