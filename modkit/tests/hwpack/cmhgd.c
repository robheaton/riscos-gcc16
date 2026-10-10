/* cmhgd.c - the modules CmhgD (swi-decoding-code: one function) and CmhgE (-DPAIR: swi-decoding-code NAME/NUMBER, two functions) of pack/module42: the kernel asks the module for the names of its SWIs.
     *CmhgD_Test / *CmhgE_Test   OS_SWINumberFromString and OS_SWINumberToString for the module's SWIs, and what the module's functions saw (the stack alignment, the private word) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kernel.h>
#include <swis.h>
#include "header.h"

#ifdef PAIR
#define MOD "CmhgE"
#define CHUNK 0x58E40
#define INIT ce_init
#define SWI ce_swi
#define TABLE ce_table
#else
#define MOD "CmhgD"
#define CHUNK 0x58E00
#define INIT cd_init
#define SWI cd_swi
#define TABLE cd_table
#endif

static void *my_pw;
static volatile int seen_calls, seen_sp7 = -1, seen_pw_ok = -1;

_kernel_oserror *INIT (const char *tail, int podule, void *pw) { (void) tail; (void) podule; my_pw = pw; return 0; }
_kernel_oserror *SWI (int number, _kernel_swi_regs *r, void *pw) { (void) pw; r->r[0] = 4000 + number; return 0; }

/* the names: MOD_Zed is 7, MOD_Nine is 9.  The kernel gives the decoding code the WHOLE name with its prefix, and the code writes the whole name */
static int name_to_number (const char *n)
{
  const char *p = MOD "_";
  while (*p) if (*n++ != *p++) return -1;
  if (n[0] == 'Z' && n[1] == 'e' && n[2] == 'd' && n[3] <= ' ') return 7;
  if (n[0] == 'N' && n[1] == 'i' && n[2] == 'n' && n[3] == 'e' && n[4] <= ' ') return 9;
  return -1;
}
static int number_to_name (int number, char *buffer, int offset, int limit)
{
  const char *name = number == 7 ? MOD "_Zed" : number == 9 ? MOD "_Nine" : 0;
  int n = 0;
  if (!name) return offset;
  while (name[n] && offset + n < limit) { buffer[offset + n] = name[n]; n++; }
  return offset + n;
}

#ifdef PAIR
int ce_n2n (const char *name, void *pw)
{
  unsigned sp;
  __asm__ volatile ("mov %0, sp" : "=r" (sp));
  seen_calls++; seen_sp7 = (int) (sp & 7); seen_pw_ok = pw == my_pw;
  return name_to_number (name);
}
int ce_n2s (int number, char *buffer, int offset, int limit, void *pw)
{
  unsigned sp;
  __asm__ volatile ("mov %0, sp" : "=r" (sp));
  seen_calls++; seen_sp7 = (int) (sp & 7); seen_pw_ok = pw == my_pw;
  return number_to_name (number, buffer, offset, limit);
}
#else
int cd_dec (_kernel_swi_regs *r, void *pw)
{
  unsigned sp;
  __asm__ volatile ("mov %0, sp" : "=r" (sp));
  seen_calls++; seen_sp7 = (int) (sp & 7); seen_pw_ok = pw == my_pw;
  if (r->r[0] < 0) r->r[0] = name_to_number ((const char *) r->r[1]);
  else r->r[2] = number_to_name (r->r[0], (char *) r->r[1], r->r[2], r->r[3]);
  return 0;
}
#endif

static int fails, checks;
static void check (int ok, const char *what) { checks++; if (!ok) { fails++; printf ("  FAIL  %s\n", what); } else printf ("  ok    %s\n", what); }

_kernel_oserror *TABLE (const char *arg_string, int argc, int number, void *pw)
{
  _kernel_oserror *e;
  int n = 0, used = 0;
  char buf[64];
  (void) arg_string; (void) argc; (void) number; (void) pw;
  fails = checks = 0;
  printf (MOD "_Test (SVC mode)\n");
  seen_calls = 0;
  e = _swix (0x39, _IN (1) | _OUT (0), MOD "_Zed", &n);
  check (e == 0 && n == CHUNK + 7, "OS_SWINumberFromString " MOD "_Zed = chunk + 7");
  if (e) printf ("        (the error: &%X \"%s\")\n", (unsigned) e->errnum, e->errmess);
  check (seen_calls >= 1 && seen_pw_ok == 1, "the kernel called the module's decoding code, which got the module's private word");
  printf ("  INFO  the decoding code saw sp & 7 = %d inside its own body (after its prologue, so 4 is normal: the veneer aligns sp before the call), %d call%s\n", seen_sp7, seen_calls, seen_calls == 1 ? "" : "s");
  e = _swix (0x39, _IN (1) | _OUT (0), MOD "_Nine", &n);
  check (e == 0 && n == CHUNK + 9, "OS_SWINumberFromString " MOD "_Nine = chunk + 9");
  e = _swix (0x39, _IN (1) | _OUT (0), MOD "_Foo", &n);
  check (e != 0, "OS_SWINumberFromString " MOD "_Foo is not known");
  if (e) printf ("        (the error: &%X \"%s\")\n", (unsigned) e->errnum, e->errmess);
  memset (buf, '.', sizeof buf);
  e = _swix (0x38, _INR (0, 2) | _OUT (2), CHUNK + 7, buf, (unsigned) sizeof buf, &used);
  check (e == 0 && used >= 7 && !strncmp (buf, MOD "_Zed", 8), "OS_SWINumberToString chunk + 7 = " MOD "_Zed");
  if (e) printf ("        (the error: &%X \"%s\")\n", (unsigned) e->errnum, e->errmess);
  else printf ("  INFO  OS_SWINumberToString chunk + 7: %d bytes, \"%.12s\"\n", used, buf);
  memset (buf, '.', sizeof buf);
  e = _swix (0x38, _INR (0, 2) | _OUT (2), CHUNK + 5, buf, (unsigned) sizeof buf, &used);
  printf ("  INFO  OS_SWINumberToString chunk + 5 (the module does not know it): %s%s, %d bytes, \"%.16s\"\n", e ? "error " : "no error", e ? e->errmess : "", e ? 0 : used, e ? "" : buf);
  { int r0 = 0; e = _swix (CHUNK, _IN (0) | _OUT (0), 5, &r0); check (e == 0 && r0 == 4000, "SWI " MOD "_00: the SWI handler (r0 = 4000)"); }
  printf (MOD "_Test: %d checks, %d FAILED\n", checks, fails);
  return 0;
}
