/* kv64test - the SWIs and the commands of Kv64.  RMLoad Kv64 first. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);
#define KV 0x59C40
static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf ("FAIL %s (line %d)\n", #c, __LINE__); } else printf ("PASS %s\n", #c); } while (0)
static unsigned long kv (int n, unsigned long a, unsigned long b, unsigned long c, unsigned long out[10])
{
  unsigned long r[10] = { a, b, c };
  unsigned long e = swi_x (KV + n, r);
  if (out) memcpy (out, r, sizeof r);
  return e;
}
int main (void)
{
  unsigned long r[10]; char buf[64];
  printf ("RISC OS 64 test kv64 (a key/value module)\n");
  CHECK (kv (3, 0, 0, 0, r) == 0 && r[0] == 0);
  CHECK (kv (0, (unsigned long) "alpha", (unsigned long) "one", 0, r) == 0 && r[0] == 1);
  CHECK (kv (0, (unsigned long) "beta", (unsigned long) "two words", 0, r) == 0 && r[0] == 1);
  CHECK (kv (0, (unsigned long) "gamma", (unsigned long) "3", 0, r) == 0 && r[0] == 1);
  CHECK (kv (3, 0, 0, 0, r) == 0 && r[0] == 3);
  CHECK (kv (0, (unsigned long) "alpha", (unsigned long) "uno", 0, r) == 0 && r[0] == 0);			/* same length: in place */
  CHECK (kv (0, (unsigned long) "beta", (unsigned long) "dos", 0, r) == 0 && r[0] == 0);			/* shorter: replaced */
  CHECK (kv (3, 0, 0, 0, r) == 0 && r[0] == 3);
  CHECK (kv (1, (unsigned long) "alpha", 0, 0, r) == 0 && r[2] == 4);					/* only the size */
  CHECK (kv (1, (unsigned long) "alpha", (unsigned long) buf, sizeof buf, r) == 0 && !strcmp (buf, "uno") && r[2] == 4);
  CHECK (kv (1, (unsigned long) "beta", (unsigned long) buf, sizeof buf, r) == 0 && !strcmp (buf, "dos"));
  CHECK (kv (1, (unsigned long) "alpha", (unsigned long) buf, 2, r) != 0);				/* too small */
  CHECK (kv (1, (unsigned long) "nosuch", (unsigned long) buf, sizeof buf, r) != 0);
  CHECK (kv (0, 0, 0, 0, r) != 0);									/* no key */
  { int found = 0, i; for (i = 0; i < 4; i++) { kv (4, (unsigned long) i, 0, 0, r); if (r[1]) { const char *k = (const char *) r[1]; if (!strcmp (k, "gamma") && !strcmp ((const char *) r[2], "3")) found++; } else if (i < 3) found = -100; }
    CHECK (found == 1); kv (4, 3, 0, 0, r); CHECK (r[1] == 0); }
  CHECK (kv (2, (unsigned long) "beta", 0, 0, r) == 0 && r[0] == 1);
  CHECK (kv (2, (unsigned long) "beta", 0, 0, r) == 0 && r[0] == 0);
  CHECK (kv (3, 0, 0, 0, r) == 0 && r[0] == 2);
  { int i; char k[16]; for (i = 0; i < 200; i++) { snprintf (k, sizeof k, "key%d", i); kv (0, (unsigned long) k, (unsigned long) k, 0, r); } }
  CHECK (kv (3, 0, 0, 0, r) == 0 && r[0] == 202);
  CHECK (kv (5, 0, 0, 0, r) == 0 && r[0] == 202);
  CHECK (kv (3, 0, 0, 0, r) == 0 && r[0] == 0);
  printf ("SUMMARY %d failed\n", fails);
  return fails != 0;
}
