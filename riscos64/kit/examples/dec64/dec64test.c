/* dec64test - tests Dec64: OS_SWINumberFromString, OS_SWINumberToString, the SWIs and the error from error-identifiers. */
#include <stdio.h>
#include <string.h>
extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);
static int checks, fails;
#define CHECK(c, what) do { checks++; if (!(c)) { fails++; printf ("FAIL %s (line %d)\n", what, __LINE__); } else printf ("PASS %s\n", what); } while (0)

int main (void)
{
  char buf[64] = "";
  unsigned long r[10];
  printf ("RISC OS 64 test dec64\n");
  memset (r, 0, sizeof r); r[1] = (unsigned long) "Dec64_Beta";
  { unsigned long e = swi_x (0x39, r); printf ("INFO OS_SWINumberFromString \"Dec64_Beta\": V=%lu r0=&%lX (&59AC1 if the kernel uses the module's decoding code or table; Pyromaniac uses the table only)\n", e, r[0] & 0xFFFFFFFF); }
  memset (r, 0, sizeof r); r[1] = (unsigned long) "Dec64_Nothing";
  { unsigned long e = swi_x (0x39, r); printf ("INFO OS_SWINumberFromString of an unknown name: V=%lu\n", e); CHECK (e == 1, "an unknown name is an error"); }
  memset (r, 0, sizeof r); r[0] = 0x59AC0; r[1] = (unsigned long) buf; r[2] = sizeof buf;
  { unsigned long e = swi_x (0x38, r); printf ("INFO OS_SWINumberToString &59AC0: V=%lu \"%s\" r2=%lu\n", e, buf, r[2]); }
  memset (r, 0, sizeof r);
  CHECK (!swi_x (0x59AC0, r) && (r[0] & 0xFFFFFFFF) == 42, "Dec64_Alpha: r0 = 42");
  memset (r, 0, sizeof r);
  { unsigned long e = swi_x (0x59AC1, r); const unsigned *b = (const unsigned *) r[0];
    CHECK (e == 1 && b[0] == 0x20D00 && !strcmp ((const char *) (b + 1), "Dec64: this error comes from error-identifiers"), "Dec64_Beta: the error err_Bad (&20D00, from error-identifiers) is readable"); }
  memset (r, 0, sizeof r);
  CHECK (!swi_x (0x59AC2, r) && (r[0] & 0xFFFFFFFF) == 1, "Dec64_SelfTest: the module's decoding entry answers both directions (and -1 for an unknown name)");
  printf ("SUMMARY %d checks, %d failed\n", checks, fails);
  return fails != 0;
}
