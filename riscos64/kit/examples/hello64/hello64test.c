/* hello64test - a program that uses the module Hello64 (kit/examples/hello64): its SWIs, its commands, its service call and its errors.  Built with aarch64-riscos-gcc (a program, not -mmodule). */
#include <stdio.h>
#include <string.h>

extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);

#define Hello64_Add 0x59A80
#define Hello64_Info 0x59A81
#define Hello64_Fail 0x59A82
#define OS_CLI 0x05
#define OS_ServiceCall 0x30
static int checks, fails;
#define CHECK(c, what) do { checks++; if (!(c)) { fails++; printf ("FAIL %s (line %d)\n", what, __LINE__); } else printf ("PASS %s\n", what); } while (0)
static unsigned errnum (unsigned long *r) { return ((const unsigned *) r[0])[0]; }
static const char *errtext (unsigned long *r) { return (const char *) r[0] + 4; }

int main (void)
{
  unsigned long r[10] = { 5, 7 };
  printf ("RISC OS 64 test hello64test (the module Hello64)\n");
  CHECK (!swi_x (Hello64_Add, r) && (r[0] & 0xFFFFFFFF) == 12, "Hello64_Add: 5 + 7 = 12");
  { unsigned long q[10] = { 0 };
    CHECK (!swi_x (Hello64_Info, q) && (q[0] & 0xFFFFFFFF) == 2 && (q[1] & 0xFFFFFFFF) == 0 && (q[2] & 0xFFFFFFFF) == 0 && (q[3] & 0xFFFFFFFF) == 3 && (q[4] & 0xFFFFFFFF) == 1, "Hello64_Info: 2 calls, no service calls, bss zero, the table of pointers right");
    printf ("INFO r0-r4 = %lu %lu %lu %lu %lu\n", q[0] & 0xFFFFFFFF, q[1] & 0xFFFFFFFF, q[2] & 0xFFFFFFFF, q[3] & 0xFFFFFFFF, q[4] & 0xFFFFFFFF); }
  { unsigned long q[10] = { 0 };
    CHECK (swi_x (Hello64_Fail, q) == 1 && errnum (q) == 0x20B01 && !strcmp (errtext (q), "Hello64: this error is in the module's code"), "Hello64_Fail: the error from the module's code is readable (copied to the RMA)"); }
  { unsigned long q[10] = { 0 };
    CHECK (swi_x (Hello64_Add + 3, q) == 1 && errnum (q) == 0x1E6, "an unknown SWI of the chunk: the kernel's own error &1E6"); }
  { const char *c = "Hello64_Say Fred"; unsigned long q[10] = { (unsigned long) c };
    CHECK (!swi_x (OS_CLI, q), "*Hello64_Say Fred (the module prints a greeting)"); }
  { const char *c = "Hello64_Say"; unsigned long q[10] = { (unsigned long) c };
    CHECK (!swi_x (OS_CLI, q), "*Hello64_Say (no parameter)"); }
  { const char *c = "Hello64_Error"; unsigned long q[10] = { (unsigned long) c };
    CHECK (swi_x (OS_CLI, q) == 1 && errnum (q) == 0x20B02 && !strcmp (errtext (q), "Hello64: this error was made with mk_error"), "*Hello64_Error: an error made with mk_error"); }
  { unsigned long q[10] = { 0, 0x59A81 };                     /* OS_ServiceCall with the number the module's table lists (&59A81): r1 = the number */
    swi_x (OS_ServiceCall, q); }
  { unsigned long q[10] = { 0 };
    CHECK (!swi_x (Hello64_Info, q) && (q[0] & 0xFFFFFFFF) == 5 && (q[1] & 0xFFFFFFFF) == 1 && (q[2] & 0xFFFFFFFF) == 2, "Hello64_Info: 5 calls, one service call counted, the history (bss) holds the 2 from the first Info");
    printf ("INFO r0-r4 = %lu %lu %lu %lu %lu\n", q[0] & 0xFFFFFFFF, q[1] & 0xFFFFFFFF, q[2] & 0xFFFFFFFF, q[3] & 0xFFFFFFFF, q[4] & 0xFFFFFFFF); }
  printf ("SUMMARY %d checks, %d failed\n", checks, fails);
  return fails != 0;
}
