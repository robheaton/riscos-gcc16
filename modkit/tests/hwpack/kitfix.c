/* kitfix.c - the module KitFix of pack/module43: the faults of the kit that 16.2.0-18 fixed, on the machine.
     *KitFix_Test   1. a weak undefined function and an absolute symbol (_Lib$Reloc$Off$DP) are 0 in the running module (modreloc used to add the load address to them)
                    2. __current_sp () and _sprintf (the names that the OS sources use)
                    3. xos_read_var_val_size: the veneer (R1 = 0, R2 = 0x80000000 from the comment of OSLib) gives the same answer as the raw SWI
                    4. xosfile_load: the veneer (R3 = 1: load at the file's own address) loads a file where the file says, which is the module's buffer */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kernel.h>
#include <swis.h>
#include "oslib/osfile.h"
#include "oslib/os.h"
#include "header.h"

extern void kf_weak_fn (void) __attribute__ ((weak));
extern char kf_abs[] __asm__ ("_Lib$Reloc$Off$DP");
extern unsigned __current_sp (void);
extern int _sprintf (char *, const char *, ...);

void *kf_weak_ptr = (void *) kf_weak_fn;
void *kf_abs_ptr = (void *) kf_abs;

static int fails, checks;
static void check (int ok, const char *what) { checks++; if (!ok) { fails++; printf ("  FAIL  %s\n", what); } else printf ("  ok    %s\n", what); }

_kernel_oserror *kf_init (const char *tail, int podule, void *pw) { (void) tail; (void) podule; (void) pw; return 0; }

_kernel_oserror *kf_table (const char *arg_string, int argc, int number, void *pw)
{
  char buf[64];
  _kernel_oserror *e;
  os_error *oe;
  (void) arg_string; (void) argc; (void) number; (void) pw;
  fails = checks = 0;
  printf ("KitFix_Test\n");
  check (kf_weak_ptr == 0 && (void *) kf_weak_fn == 0, "an undefined weak function is 0 (modreloc did not add the load address to its word)");
  check (kf_abs_ptr == 0, "_Lib$Reloc$Off$DP is the absolute value 0");
  {
    unsigned local, sp = __current_sp ();
    unsigned d = sp > (unsigned) &local ? sp - (unsigned) &local : (unsigned) &local - sp;
    check (d < 4096 && (sp & 3) == 0, "__current_sp () is the stack pointer of the caller");
  }
  _sprintf (buf, "%d-%s", 42, "x");
  check (strcmp (buf, "42-x") == 0, "_sprintf ()");
  /* xos_read_var_val_size: set a variable of 5 characters; the veneer (R1 = 0, R2 = 0x80000000 from the comment) must do what the SWI with those registers does; the SWI without them (what the veneer of 16.2.0-17 did) is shown */
  e = _swix (0x24, _INR (0, 4), "KitFix$Var", "hello", 5, 0, 0);                              /* OS_SetVarVal */
  check (e == 0, "OS_SetVarVal KitFix$Var");
  {
    int used = 0, ctx = 0, raw2 = 0, rawctx = 0, ty = 0, old2 = 0;
    os_var_type vt = 0;
    _kernel_oserror *e2, *e3;
    unsigned en_v = 0, en_r = 0;
    oe = xos_read_var_val_size ("KitFix$Var", 0, os_VARTYPE_STRING, &used, &ctx, &vt);
    en_v = oe ? (unsigned) oe->errnum : 0;
    e2 = _swix (0x23, _INR (0, 4) | _OUT (2) | _OUT (3) | _OUT (4), "KitFix$Var", 0, 0x80000000u, 0, 0, &raw2, &rawctx, &ty);   /* OS_ReadVarVal */
    en_r = e2 ? (unsigned) e2->errnum : 0;
    e3 = _swix (0x23, _INR (0, 4) | _OUT (2), "KitFix$Var", 0, 0, 0, 0, &old2);              /* without the constants */
    printf ("  INFO  veneer: error &%X, used %d, context %d, type %d\n", en_v, used, ctx, (int) vt);
    printf ("  INFO  SWI with R1 = 0, R2 = 0x80000000: error &%X, r2 %d, context %d, type %d\n", en_r, raw2, rawctx, ty);
    printf ("  INFO  SWI with R1 = 0, R2 = 0 (the veneer of 16.2.0-17): error &%X, r2 %d\n", e3 ? (unsigned) e3->errnum : 0, old2);
    check (en_v == en_r && (en_r != 0 || used == raw2), "the veneer and the SWI with the constants give the same answer");
  }
  _swix (0x24, _INR (0, 4), "KitFix$Var", 0, -1, 0, 0);                                      /* remove it */
  /* xosfile_load: a file that says where it is to be loaded (our buffer), saved with the SWI, cleared, loaded by the veneer */
  {
    char *area = malloc (64), *pat = malloc (64);
    fileswitch_object_type ot = 0;
    bits load = 0, exec = 0;
    int size = 0, i, same;
    fileswitch_attr attr = 0;
    const char *name = "<Wimp$ScrapDir>.KitFixFile";
    char path[200];
    if (!area || !pat) { check (0, "malloc"); return 0; }
    for (i = 0; i < 64; i++) pat[i] = (char) (i * 3 + 1);
    memcpy (area, pat, 64);
    e = _swix (0x08, _INR (0, 5), 10, name, (unsigned) area, (unsigned) area, (unsigned) area, (unsigned) area + 64);          /* OS_File 10: save, load = exec = the buffer */
    check (e == 0, "OS_File 10 saves a file that wants to be loaded at the buffer");
    if (e) printf ("        (the error: &%X \"%s\")\n", (unsigned) e->errnum, e->errmess);
    memset (area, 0, 64);
    sprintf (path, "%s", name);
    oe = xosfile_load (path, &ot, &load, &exec, &size, &attr);
    if (oe) printf ("        (the veneer: &%X \"%s\")\n", (unsigned) oe->errnum, oe->errmess);
    same = memcmp (area, pat, 64) == 0;
    printf ("  INFO  veneer: type %d, load %08X, exec %08X, size %d, buffer %s\n", (int) ot, (unsigned) load, (unsigned) exec, size, same ? "has the pattern" : "is not the pattern");
    memset (area, 0, 64);
    {
      unsigned rt = 0, rl = 0, rx = 0; int rs = 0;
      e = _swix (0x08, _INR (0, 3) | _OUT (0) | _OUT (2) | _OUT (3) | _OUT (4), 255, name, 0, 1, &rt, &rl, &rx, &rs);        /* OS_File 255 with R3 = 1: the file's own address */
      if (e) printf ("        (the SWI: &%X \"%s\")\n", (unsigned) e->errnum, e->errmess);
      printf ("  INFO  SWI with R3 = 1: type %u, load %08X, exec %08X, size %d, buffer %s\n", rt, rl, rx, rs, memcmp (area, pat, 64) == 0 ? "has the pattern" : "is not the pattern");
      check ((oe == 0) == (e == 0) && (oe == 0 || (int) oe->errnum == (int) e->errnum) && (oe != 0 || ((unsigned) load == rl && size == rs)), "xosfile_load does what OS_File 255 with R3 = 1 does");
    }
    _swix (0x08, _INR (0, 1), 6, name);                                                         /* delete */
    free (area); free (pat);
  }
  printf ("KitFix_Test: %d checks, %d FAILED\n", checks, fails);
  return 0;
}
