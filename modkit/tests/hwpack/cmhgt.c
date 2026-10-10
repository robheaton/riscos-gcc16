/* cmhgt.c - the module CmhgT of pack/module42: the CMHG options of 16.2.0-18, checked on the machine (the simulation of the same module is tests/sim-cmhgt.py).
     *CmhgT_Test       everything below in SVC mode: the SWIs with functions of their own and the one that goes to swi-handler-code, the names of the SWIs, the commands (the function of the table, handler:,
                       no-handler:), the generic veneers with private-word: and carry-capable: (mk_call_veneer, veneercall.S), the vector veneers with error-capable: (ct_call_vector, vecall.S)
     *CmhgT_Table, *CmhgT_Own, *CmhgT_Help   the three kinds of command;   *CmhgT_Try <command>   runs a command and prints its error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kernel.h>
#include <swis.h>
#include "header.h"

extern unsigned mk_call_veneer (const unsigned in[13], unsigned out[13], void (*veneer) (void), unsigned flags);
extern unsigned ct_call_vector (const unsigned in[13], unsigned out[13], void (*veneer) (void), unsigned flags, unsigned *claimed);

static _kernel_oserror err_a = { 0x58DA, "CmhgT error A" };
static _kernel_oserror err_b = { 0x58DB, "CmhgT error B" };
static void *my_pw;

_kernel_oserror *ct_init (const char *tail, int podule, void *pw) { (void) tail; (void) podule; my_pw = pw; return 0; }

/* SWIs: CmhgT_One and CmhgT_Three have their own functions, CmhgT_Two and the offsets past the table go to ct_swi */
_kernel_oserror *ct_one (int number, _kernel_swi_regs *r, void *pw) { (void) pw; r->r[0] = 1000 + number; return 0; }
_kernel_oserror *ct_three (int number, _kernel_swi_regs *r, void *pw) { (void) pw; r->r[0] = 3000 + number; return 0; }
_kernel_oserror *ct_swi (int number, _kernel_swi_regs *r, void *pw)
{
  (void) pw;
  if (number == 1) { r->r[0] = 2000 + number; return 0; }
  return error_BAD_SWI;
}

/* generic veneers.  ct_pw: private-word: r4, the handler copies the private word to r8 and r1 of the block to r9.  ct_cc: carry-capable: r0 = 0 nothing, 1 an error, 2 VENEER_SETCARRY.
   ct_both: the private word in r0, r1 = what to answer.  ct_plain: no options, r0 = 2 is answered as 2 (an error pointer) */
_kernel_oserror *ct_pw_handler (_kernel_swi_regs *r, void *pw) { r->r[8] = (int) pw; r->r[9] = r->r[1]; return 0; }
_kernel_oserror *ct_cc_handler (_kernel_swi_regs *r, void *pw)
{
  (void) pw;
  r->r[7] = 77;
  return r->r[0] == 2 ? VENEER_SETCARRY : r->r[0] == 1 ? &err_a : 0;
}
_kernel_oserror *ct_both_handler (_kernel_swi_regs *r, void *pw) { r->r[8] = (int) pw; return r->r[1] == 2 ? VENEER_SETCARRY : r->r[1] == 1 ? &err_b : 0; }
_kernel_oserror *ct_plain_handler (_kernel_swi_regs *r, void *pw) { r->r[8] = (int) pw; return r->r[0] == 2 ? VENEER_SETCARRY : 0; }

/* vectors: r1 of the block = 0 claim, 1 pass on, 2 an error (ct_vec: VECTOR_ERROR; ct_vecn is not error-capable: 2 passes on) */
int ct_vec_handler (_kernel_swi_regs *r, void *pw) { r->r[8] = (int) pw; return r->r[1] == 2 ? VECTOR_ERROR (&err_a) : r->r[1]; }
int ct_vecn_handler (_kernel_swi_regs *r, void *pw) { r->r[8] = (int) pw; return r->r[1]; }

/* commands */
static char last[64];
static int fails, checks;
static void check (int ok, const char *what) { checks++; if (!ok) { fails++; printf ("  FAIL  %s\n", what); } else printf ("  ok    %s\n", what); }

static unsigned flagsets[4] = { 0x00000000u, 0xA0000000u, 0x50000000u, 0xF0000000u };            /* NZCV: 0000, 1010, 0101, 1111 */
static void flagname (char *b, unsigned f) { sprintf (b, "NZCV=%u%u%u%u", (f >> 31) & 1, (f >> 30) & 1, (f >> 29) & 1, (f >> 28) & 1); }

static void test_swis (void)
{
  _kernel_oserror *e;
  int n = 0;
  char buf[64];
  int used = 0;
  e = _swix (0x39, _IN (1) | _OUT (0), "CmhgT_Two", &n);
  check (e == 0 && n == 0x58DC1, "OS_SWINumberFromString CmhgT_Two = &58DC1");
  e = _swix (0x38, _INR (0, 2) | _OUT (2), 0x58DC2, buf, (unsigned) sizeof buf, &used);
  check (e == 0 && used > 0 && !strncmp (buf, "CmhgT_Three", 11), "OS_SWINumberToString &58DC2 = CmhgT_Three (the /function does not belong to the name)");
  { int r0 = 0; e = _swix (0x58DC0, _IN (0) | _OUT (0), 5, &r0); check (e == 0 && r0 == 1000, "SWI CmhgT_One: its own function ct_one (r0 = 1000 + the offset 0)"); }
  { int r0 = 0; e = _swix (0x58DC1, _IN (0) | _OUT (0), 5, &r0); check (e == 0 && r0 == 2001, "SWI CmhgT_Two: no function of its own, ct_swi (r0 = 2001)"); }
  { int r0 = 0; e = _swix (0x58DC2, _IN (0) | _OUT (0), 5, &r0); check (e == 0 && r0 == 3002, "SWI CmhgT_Three: its own function ct_three (r0 = 3000 + the offset 2)"); }
  e = _swix (0x58DC3, _IN (0), 5);
  check (e != 0 && e->errnum == 0x1E6 && strstr (e->errmess, "out of range for module CmhgT"), "SWI &58DC3 (past the table): ct_swi answers error_BAD_SWI: the error of the system");
  if (e) printf ("        (the error: &%X \"%s\")\n", (unsigned) e->errnum, e->errmess);
}

static void test_commands (void)
{
  _kernel_oserror *e;
  last[0] = 0; e = _swix (0x05, _IN (0), "CmhgT_Table");
  check (e == 0 && !strcmp (last, "table 1"), "*CmhgT_Table: the function of the table, number 1");
  last[0] = 0; e = _swix (0x05, _IN (0), "CmhgT_Own");
  check (e == 0 && !strcmp (last, "own 2"), "*CmhgT_Own: its own function ct_own, number 2");
  last[0] = 0; e = _swix (0x05, _IN (0), "CmhgT_Help");
  printf ("  INFO  *CmhgT_Help (no-handler:) %s\n", e ? "gives an error" : "gives no error");
  if (e) printf ("        (the error: &%X \"%s\")\n", (unsigned) e->errnum, e->errmess);
  check (last[0] == 0, "*CmhgT_Help has no code: no function ran");
  printf ("  INFO  *Help CmhgT_Help (the kernel prints the help text):\n");
  _swix (0x05, _IN (0), "Help CmhgT_Help");
}

static void test_generics (void)
{
  int f;
  char tag[40];
  for (f = 0; f < 4; f++)
    {
      unsigned in[13], out[13], fl, want, i, a;
      flagname (tag, flagsets[f]);
      /* ct_pw: the private word is in r4; r12 is the caller's and must come back */
      for (i = 0; i < 13; i++) in[i] = 0x10000u + 0x111u * i;
      in[4] = 0x43484D54u; in[12] = 0xC1C1C1C1u; in[1] = 0x55;
      fl = mk_call_veneer (in, out, ct_pw, flagsets[f]);
      {
        int ok = out[8] == 0x43484D54u && out[9] == 0x55 && out[12] == 0xC1C1C1C1u && out[4] == in[4] && fl == flagsets[f];
        for (i = 0; i < 12; i++) if (i != 8 && i != 9 && out[i] != in[i]) ok = 0;
        { char b[200]; sprintf (b, "ct_pw (%s): the handler got r4 as its private word, r12 and the other registers come back, flags kept", tag); check (ok, b); }
      }
      /* ct_cc: 0 nothing, 2 = VENEER_SETCARRY, 1 = an error */
      for (a = 0; a < 3; a++)
        {
          int ok;
          for (i = 0; i < 13; i++) in[i] = 0x20000u + 0x113u * i;
          in[0] = a; in[12] = 0xC1C1C1C1u;
          fl = mk_call_veneer (in, out, ct_cc, flagsets[f]);
          want = a == 0 ? flagsets[f] : a == 2 ? ((flagsets[f] | 0x20000000u) & ~0x10000000u) : (flagsets[f] | 0x10000000u);
          ok = out[7] == 77 && fl == want;
          for (i = 1; i < 12; i++) if (i != 7 && out[i] != in[i]) ok = 0;
          if (a != 1 && out[0] != in[0]) ok = 0;
          if (a == 1 && (void *) out[0] != (void *) &err_a) ok = 0;
          { char b[200]; sprintf (b, "ct_cc (%s), the handler answers %s", tag, a == 0 ? "0" : a == 2 ? "VENEER_SETCARRY: C set, V clear" : "an error: V set, r0 = the block"); check (ok, b); }
        }
      /* ct_both: the private word in r0, r1 = what to answer */
      for (a = 0; a < 3; a++)
        {
          int ok;
          for (i = 0; i < 13; i++) in[i] = 0x30000u + 0x117u * i;
          in[0] = 0x43484D54u; in[1] = a; in[12] = 0xC1C1C1C1u;
          fl = mk_call_veneer (in, out, ct_both, flagsets[f]);
          want = a == 0 ? flagsets[f] : a == 2 ? ((flagsets[f] | 0x20000000u) & ~0x10000000u) : (flagsets[f] | 0x10000000u);
          ok = out[8] == 0x43484D54u && fl == want && out[12] == 0xC1C1C1C1u;
          for (i = 1; i < 12; i++) if (i != 8 && out[i] != in[i]) ok = 0;
          if (a != 1 && out[0] != in[0]) ok = 0;
          if (a == 1 && (void *) out[0] != (void *) &err_b) ok = 0;
          { char b[200]; sprintf (b, "ct_both (%s), private word in r0, the handler answers %s", tag, a == 0 ? "0" : a == 2 ? "VENEER_SETCARRY" : "an error"); check (ok, b); }
        }
      /* ct_plain: no carry-capable:, so 2 is an error pointer: V set, r0 = 2 */
      for (i = 0; i < 13; i++) in[i] = 0x40000u + 0x119u * i;
      in[0] = 2;
      fl = mk_call_veneer (in, out, ct_plain, flagsets[f]);
      { char b[200]; sprintf (b, "ct_plain (%s): without carry-capable: the answer 2 is an error (V set, r0 = 2)", tag); check (out[0] == 2 && fl == (flagsets[f] | 0x10000000u), b); }
    }
}

static void test_vectors (void)
{
  int f;
  unsigned a;
  char tag[40];
  for (f = 0; f < 4; f++)
    {
      flagname (tag, flagsets[f]);
      for (a = 0; a < 3; a++)
        {
          unsigned in[13], out[13], fl, claimed = 9, i;
          int ok;
          for (i = 0; i < 13; i++) in[i] = 0x50000u + 0x11Bu * i;
          in[1] = a; in[12] = (unsigned) my_pw;
          fl = ct_call_vector (in, out, ct_vec, flagsets[f], &claimed);
          ok = out[8] == (unsigned) my_pw && claimed == (a == 1 ? 0u : 1u) && fl == (a == 2 ? (flagsets[f] | 0x10000000u) : flagsets[f]);
          for (i = 2; i < 12; i++) if (i != 8 && out[i] != in[i]) ok = 0;
          if (a == 2 && (void *) out[0] != (void *) &err_a) ok = 0;
          if (a != 2 && (out[0] != in[0] || out[1] != in[1])) ok = 0;
          { char b[200]; sprintf (b, "ct_vec (%s), the handler says %s", tag, a == 0 ? "claim" : a == 1 ? "pass on" : "VECTOR_ERROR (&error): claim with V set"); check (ok, b); }
        }
      for (a = 0; a < 3; a++)
        {
          unsigned in[13], out[13], fl, claimed = 9, i;
          int ok;
          for (i = 0; i < 13; i++) in[i] = 0x60000u + 0x11Du * i;
          in[1] = a; in[12] = (unsigned) my_pw;
          fl = ct_call_vector (in, out, ct_vecn, flagsets[f], &claimed);
          ok = claimed == (a == 0 ? 1u : 0u) && fl == flagsets[f] && out[8] == (unsigned) my_pw;
          for (i = 0; i < 12; i++) if (i != 8 && out[i] != in[i]) ok = 0;
          { char b[200]; sprintf (b, "ct_vecn (%s), the handler says %s: %s, flags kept", tag, a == 0 ? "claim" : a == 1 ? "pass on" : "2 (not an error here)", a == 0 ? "claimed" : "passed on"); check (ok, b); }
        }
    }
}

_kernel_oserror *ct_own (const char *arg_string, int argc, int number, void *pw)
{
  (void) arg_string; (void) argc; (void) pw;
  sprintf (last, "own %d", number);
  return 0;
}

_kernel_oserror *ct_table (const char *arg_string, int argc, int number, void *pw)
{
  (void) argc; (void) pw;
  switch (number)
    {
    case 0:
      fails = checks = 0;
      printf ("CmhgT_Test (SVC mode)\n");
      test_swis ();
      test_commands ();
      test_generics ();
      test_vectors ();
      printf ("CmhgT_Test: %d checks, %d FAILED\n", checks, fails);
      return 0;
    case 4:
      {
        _kernel_oserror *e = _swix (0x05, _IN (0), arg_string);
        if (e) printf ("error &%X \"%s\"\n", (unsigned) e->errnum, e->errmess); else printf ("no error\n");
        return 0;
      }
    default:
      sprintf (last, "table %d", number);
      return 0;
    }
}
