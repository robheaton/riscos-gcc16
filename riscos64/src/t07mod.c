/* t07mod - the SWIs of the module Mod64 (module64.S): look up their numbers with OS_SWINumberFromString and call them.  Run after RMLoad Mod64.
   Mod64_Add reads the register block of the SWI handler as slots of 64 bits, Mod64_Add32 as slots of 32 bits: the test finds out which one the OS uses. */
#include "t64.h"
extern u64 swi_x (u64 swi, u64 *regs);
extern u64 swi_f (u64 swi, u64 *regs);                   /* the flags N=8 Z=4 C=2 V=1 that the SWI left */

static int lookup (const char *name, u64 *num)
{
  u64 r[10] = { 0 };
  int v;
  r[1] = (u64) name;
  v = (int) swi_x (0x39, r);                              /* OS_SWINumberFromString */
  if (v) { const u32 *e = (const u32 *) r[0]; t_puts ("INFO OS_SWINumberFromString: "); t_puts ((const char *) (e + 1)); t_putc ('\n'); return 0; }
  *num = r[0];
  return 1;
}

#define ERRTEXT "Mod64: this error is on purpose"

static void kv (const char *name, const char *what, u64 val)   /* INFO name: what = &val */
{
  char k[96]; u64 n = strlen (name), m = strlen (what);
  memcpy (k, name, n); k[n] = ':'; k[n + 1] = ' '; memcpy (k + n + 2, what, m + 1);
  t_info (k, val);
}

/* call a SWI of the module that returns an error block; show what comes back without reading the block in this program (a program at EL0 may not be able to): the pointer, what the kernel makes of it
   (OS_ValidateAddress, OS_Write0 read by the kernel).  Returns V; *blk = x0 */
static int call_fail (const char *name, u64 swi, u64 *blk)
{
  u64 rr[10] = { 0 }, vr[10] = { 0 };
  int v = (int) swi_x (swi, rr);
  kv (name, "V", (u64) v); kv (name, "x0 (the error block)", rr[0]);
  *blk = rr[0];
  if (v)
  {
    u64 f;
    vr[0] = rr[0]; vr[1] = rr[0] + 36;
    f = swi_f (0x3A, vr);                                   /* OS_ValidateAddress: C set means not valid */
    kv (name, "OS_ValidateAddress flags (N=8 Z=4 C=2 V=1)", f);
    vr[0] = rr[0] + 4;
    t_puts ("INFO "); t_puts (name); t_puts (": the text, printed by the kernel with OS_Write0: ");
    if (swi_x (0x2, vr)) t_puts ("(OS_Write0 failed)");
    t_putc ('\n');
  }
  return v;
}

int main (void)
{
  u64 r[10] = { 0 };
  u64 add64 = 0, add32 = 0;
  int v, ok64, ok32;
  t_puts ("RISC OS 64 test t07mod\n");
  CHECK (lookup ("Mod64_Add", &add64));
  CHECK (lookup ("Mod64_Add32", &add32));
  if (!add64 || !add32) return t_summary ("t07mod");
  t_info ("Mod64_Add is SWI", add64);
  t_info ("Mod64_Add32 is SWI", add32);

  r[0] = 40; r[1] = 2;
  v = (int) swi_x (add64, r);
  t_info ("Mod64_Add 40 2: V", (u64) v); t_info ("Mod64_Add 40 2 gave x0", r[0]);
  ok64 = (v == 0 && r[0] == 42);
  r[0] = 40; r[1] = 2;
  v = (int) swi_x (add32, r);
  t_info ("Mod64_Add32 40 2: V", (u64) v); t_info ("Mod64_Add32 40 2 gave x0", r[0]);
  ok32 = (v == 0 && (r[0] & 0xFFFFFFFF) == 42);
  t_puts ("INFO the SWI register block has slots of "); t_puts (ok64 ? "64 bits" : ok32 ? "32 bits" : "an unknown size"); t_putc ('\n');
  CHECK (ok64 || ok32);                                   /* one of the two readings works */

  /* distinct values in all the slots: shows how the block is laid out */
  r[0] = 0x1111111111111111UL; r[1] = 0x2222222222222222UL; r[2] = 0x3333; r[3] = 0x4444;
  v = (int) swi_x (add64, r);
  t_info ("layout probe (64-bit reading): x0 after", r[0]); t_info ("x1 after", r[1]);

  {
    u64 fail = 0, carry = 0, failrma = 0, bc = 0, br = 0;
    int have = lookup ("Mod64_Fail", &fail) & lookup ("Mod64_Carry", &carry) & lookup ("Mod64_FailRMA", &failrma);
    int vc, vr;
    CHECK (have);
    if (!have) return t_summary ("t07mod");

    /* a SWI that the module does not know (Mod64 has five): the module returns -1 and the kernel raises its own error */
    {
      u64 ru[10] = { 0 };
      v = (int) swi_x (add64 + 5, ru);
      kv ("an unknown SWI of the module (offset 5)", "V", (u64) v);
      if (v) { const u32 *u = (const u32 *) ru[0]; t_info ("error number", u[0]); t_puts ("INFO error text: "); t_puts ((const char *) (u + 1)); t_putc ('\n'); }
      CHECK (v == 1);
    }

    /* the flags word of the register block (the eleventh slot): only when the slots are 64 bits, the module writes at offset 80 */
    if (ok64)
    {
      u64 rr[10] = { 0 }, f1, f0;
      rr[0] = 1; f1 = swi_f (carry, rr);
      rr[0] = 0; f0 = swi_f (carry, rr);
      t_info ("Mod64_Carry 1: flags NZCV (N=8 Z=4 C=2 V=1)", f1); t_info ("Mod64_Carry 0: flags", f0);
      CHECK ((f1 & 2) != 0);                                /* C was set by the module through the flags word */
      CHECK ((f0 & 2) == 0);                                /* and cleared by it */
    }
    else t_puts ("INFO the flags word test is skipped: it needs 64-bit slots\n");

    /* the error return of a module SWI, twice: the error block in a block of the RMA, and in the module's own code (the 0.1 / 0.2 result: this program aborted reading it).  Everything is shown first */
    vc = call_fail ("Mod64_Fail", fail, &bc);                /* the code only returns a pointer: first */
    vr = call_fail ("Mod64_FailRMA", failrma, &br);          /* this one calls OS_Module from the SWI handler */
    CHECK (vr == 1 && vc == 1);
    t_info ("the program's own address range: main", (u64) main);

    /* now read the blocks in this program, the one in the RMA first; an abort here is the answer */
    if (vr)
    {
      const u32 *e = (const u32 *) br;
      t_puts ("INFO reading the block in the RMA from this program ...\n");
      CHECK (e[0] == 0x20A01 && memcmp (e + 1, ERRTEXT, 32) == 0);
    }
    if (vc)
    {
      const u32 *e = (const u32 *) bc;
      t_puts ("INFO reading the block in the module's code from this program ...\n");
      CHECK (e[0] == 0x20A01 && memcmp (e + 1, ERRTEXT, 32) == 0);
    }
  }
  return t_summary ("t07mod");
}
