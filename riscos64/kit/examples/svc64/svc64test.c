/* svc64test - runs Svc64_Check (the kit's service entry called in both conventions from inside the module), then sends two services with OS_ServiceCall: 0x59BE1 is claimed (r1 = 0), 0x59BE2 is not. */
#include <stdio.h>
extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);
int main (void)
{
  unsigned long r[10] = { 0 };
  int fails = 0, n;
  printf ("RISC OS 64 test svc64test (the service entry of the kit)\n");
  if (swi_x (0x59C00, r)) { printf ("FAIL Svc64_Check gave an error\n"); return 1; }
  printf ("INFO checks = &%lX, services seen = %lu\n", r[0] & 0xFFFFFFFF, r[1] & 0xFFFFFFFF);
  for (n = 0; n < 8; n++) { int ok = (r[0] >> n) & 1; if (!ok) fails++; printf ("%s check %d\n", ok ? "PASS" : "FAIL", n); }
  { unsigned long q[10] = { 0xB0, 0x59BE1, 0xB2 }; int v = (int) swi_x (0x30, q);			/* OS_ServiceCall */
    printf ("%s OS_ServiceCall 0x59BE1: V = %d, r1 after = &%lX (0 = claimed)\n", (!v && q[1] == 0) ? "PASS" : "FAIL", v, q[1]); if (v || q[1]) fails++; }
  { unsigned long q[10] = { 0xB0, 0x59BE2, 0xB2 }; int v = (int) swi_x (0x30, q);
    printf ("%s OS_ServiceCall 0x59BE2: V = %d, r1 after = &%lX (not claimed)\n", (!v && q[1] == 0x59BE2) ? "PASS" : "FAIL", v, q[1]); if (v || q[1] != 0x59BE2) fails++; }
  printf ("SUMMARY %d failed\n", fails);
  return fails != 0;
}
