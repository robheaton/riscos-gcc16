/* cxx64test - tests the C++ module Cxx64: Cxx64_Info (static constructors, new/delete, virtual calls, a template, a local static) and the command. */
#include <stdio.h>
#include <string.h>
extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);
static int checks, fails;
#define CHECK(c, what) do { checks++; if (!(c)) { fails++; printf ("FAIL %s (line %d)\n", what, __LINE__); } else printf ("PASS %s\n", what); } while (0)
#define L(i) (r[i] & 0xFFFFFFFF)

int main (void)
{
  unsigned long r[10] = { 0 };
  printf ("RISC OS 64 test cxx64test (the C++ module Cxx64)\n");
  CHECK (!swi_x (0x59B00, r), "Cxx64_Info");
  printf ("INFO r0-r6 = %lu %lu %lu %lu %lu %lu %lu\n", L(0), L(1), L(2), L(3), L(4), L(5), L(6));
  CHECK (L(0) == 2, "two static objects were constructed before the module's own initialisation");
  CHECK (L(1) == 37, "virtual calls through new'd objects: 12 + 25");
  CHECK (L(2) == 2 && L(3) == 0, "two objects alive, then none after delete");
  CHECK (L(4) == 42, "a template: twice<int> (21) = 42");
  CHECK (L(5) == 100101, "a local static: 100, then 101");
  CHECK (L(6) == 0, "no static destructor has run yet");
  { const char *c = "Cxx64_Shapes"; unsigned long q[10] = { (unsigned long) c };
    CHECK (!swi_x (5, q), "*Cxx64_Shapes (prints two areas)"); }
  printf ("SUMMARY %d checks, %d failed\n", checks, fails);
  return fails != 0;
}
