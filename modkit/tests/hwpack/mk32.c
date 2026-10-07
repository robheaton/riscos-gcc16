/* mk32.c - the module of pack/module32: what the simulations cannot prove about the code that cmunge makes and the library gives, checked on the machine.
     *MK32_SelfTest           the SWIs of the module (OS_SWINumberFromString / ToString, the kernel's dispatch, a SWI error), the generic veneer called from SVC code, getenv, the heap
     *RMRun MK32 test         the same in USER mode (the module is run as a program)
     *RMRun MK32 a "b c" d    argc / argv;   *RMRun MK32 exit [n]   the exit code
     *RMRun MK32 leave        writes a file in the scrap directory and does not close it (the end of the program must);  *RMRun MK32 leavecheck   the next program: the file has the text, then it is deleted
     *MK32_After <n>          the generic veneer mk_after called by OS_CallEvery (R0 = n: every n + 1 centiseconds) in interrupt time: the mode, the stack, r12, how many calls
     *MK32_Try <command>      runs a command and prints its error (the Obey file uses it for the commands whose errors are the test)
     *Help MK32_Tokens ...    the international help and the add-syntax text, as the kernel prints them */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <kernel.h>
#include <swis.h>
#include "header.h"

#define MAGIC 0x4D4B3332u                                          /* "MK32": the handle of the generic veneers (what r12 holds, what the handler gets as pw) */
#define OS_READMONO 0x42
extern unsigned mk_call_veneer (const unsigned in[13], unsigned out[13], void (*veneer) (void), unsigned flags);

static _kernel_oserror gv_error = { 0x4D32, "MK32 generic veneer error" };
static _kernel_oserror swi_error = { 0xC0A03, "MK32 test error from SWI MKTest_Fail" };
static const char module_text[] = "MK32 0.32";

_kernel_oserror *mk_init (const char *tail, int podule_base, void *pw) { (void) tail; (void) podule_base; (void) pw; return 0; }
_kernel_oserror *mk_final (int fatal, int podule_base, void *pw)
{
  (void) fatal; (void) podule_base; (void) pw;
  _swix (0x3D, _INR (0, 1), (unsigned) mk_after, MAGIC);          /* OS_RemoveTickerEvent: nothing is left behind if *MK32_After was interrupted */
  return 0;
}

/* SWI MKTest_Add (r0 + r1), _Sub (r0 - r1), _Name (r0 -> the text, r1 its length), _Fail (an error) */
_kernel_oserror *mk_swi (int number, _kernel_swi_regs *r, void *pw)
{
  (void) pw;
  switch (number)
    {
    case 0: r->r[0] = r->r[0] + r->r[1]; return 0;
    case 1: r->r[0] = r->r[0] - r->r[1]; return 0;
    case 2: r->r[0] = (int) module_text; r->r[1] = (int) strlen (module_text); return 0;
    default: return &swi_error;
    }
}

/* the test veneer: r0 = 0: r1 = r1 * 2 + r2, r5 = pw, r6 = sp & 7, r7 = the block; r0 = 1: the same and an error; r0 = 2: nothing changes */
_kernel_oserror *mk_gv_handler (_kernel_swi_regs *r, void *pw)
{
  unsigned sp;
  __asm__ volatile ("mov %0, sp" : "=r" (sp));
  if (r->r[0] == 2) return 0;
  r->r[1] = r->r[1] * 2 + r->r[2];
  r->r[5] = (int) pw;
  r->r[6] = (int) (sp & 7);
  r->r[7] = (int) r;
  return r->r[0] == 1 ? &gv_error : 0;
}

/* the veneer that OS_CallEvery calls: counts, and notes what is wrong */
static volatile unsigned after_calls, after_bad_mode, after_irq_on, after_bad_sp, after_bad_pw;
_kernel_oserror *mk_after_handler (_kernel_swi_regs *r, void *pw)
{
  unsigned cpsr, sp;
  __asm__ volatile ("mrs %0, cpsr" : "=r" (cpsr));
  __asm__ volatile ("mov %0, sp" : "=r" (sp));
  (void) r;
  after_calls++;
  if ((cpsr & 0x1F) != 0x13) after_bad_mode++;                    /* the veneer runs the handler in SVC mode (it is entered in IRQ mode) */
  if (!(cpsr & 0x80)) after_irq_on++;                             /* with the interrupts off, as they were */
  if (sp & 7) after_bad_sp++;
  if (pw != (void *) MAGIC) after_bad_pw++;
  return 0;
}

static int fails, checks;
static void check (int ok, const char *what) { checks++; if (!ok) { fails++; printf ("  FAIL  %s\n", what); } else printf ("  ok    %s\n", what); }

static unsigned monotonic (void) { unsigned t = 0; _swix (OS_READMONO, _OUT (0), &t); return t; }

static int selftest (const char *where)
{
  _kernel_oserror *e;
  unsigned n = 0, used = 0;
  char buf[40];
  fails = checks = 0;
  printf ("MK32 self test (%s)\n", where);
  /* the decoding table: the prefix is MKTest, the title MK32 */
  e = _swix (0x39, _IN (1) | _OUT (0), "MKTest_Add", &n);
  check (e == 0 && n == 0xC0A00, "OS_SWINumberFromString MKTest_Add is &C0A00");
  e = _swix (0x39, _IN (1) | _OUT (0), "MKTest_Fail", &n);
  check (e == 0 && n == 0xC0A03, "OS_SWINumberFromString MKTest_Fail is &C0A03");
  e = _swix (0x39, _IN (1) | _OUT (0), "MK32_Add", &n);
  check (e != 0, "OS_SWINumberFromString MK32_Add is not known (the prefix is MKTest, not the title)");
  e = _swix (0x38, _INR (0, 2) | _OUT (2), 0xC0A02u, buf, (unsigned) sizeof buf, &used);
  check (e == 0 && !strcmp (buf, "MKTest_Name"), "OS_SWINumberToString &C0A02 is MKTest_Name");
  /* the kernel's own dispatch to the module */
  { int r0 = 0; e = _swix (0xC0A00, _INR (0, 1) | _OUT (0), 40, 2, &r0); check (e == 0 && r0 == 42, "SWI MKTest_Add 40 2 = 42"); }
  { int r0 = 0; e = _swix (0xC0A01, _INR (0, 1) | _OUT (0), 100, 58, &r0); check (e == 0 && r0 == 42, "SWI MKTest_Sub 100 58 = 42"); }
  { const char *t = 0; int len = 0; e = _swix (0xC0A02, _OUT (0) | _OUT (1), &t, &len); check (e == 0 && t && !strcmp (t, "MK32 0.32") && len == 9, "SWI MKTest_Name gives the text and its length"); }
  e = _swix (0xC0A03, 0);
  check (e != 0 && e->errnum == 0xC0A03 && !strcmp (e->errmess, "MK32 test error from SWI MKTest_Fail"), "SWI MKTest_Fail gives its error (V set, r0 -> the block)");
  { _kernel_oserror *l = _kernel_last_oserror (); check (l && l->errnum == 0xC0A03, "_kernel_last_oserror () has that error"); }
  /* the generic veneer, called from here with registers and flags of our choice */
  {
    static const unsigned flagsets[] = { 0x00000000u, 0x20000000u, 0x40000000u, 0x80000000u, 0xC0000000u, 0x10000000u, 0x30000000u, 0xF0000000u };
    int ok_same = 1, ok_zero = 1, ok_err = 1;
    for (unsigned f = 0; f < sizeof flagsets / sizeof flagsets[0]; f++)
      {
        unsigned in[13], out[13], fl;
        for (int i = 0; i < 12; i++) in[i] = 0x30000u + 0x1111u * (unsigned) i + f;
        in[12] = MAGIC;                                            /* r12 is the handle: the handler gets it as pw */
        in[0] = 2;
        fl = mk_call_veneer (in, out, mk_gv, flagsets[f]);
        for (int i = 0; i < 12; i++) if (out[i] != in[i]) ok_same = 0;
        if (fl != flagsets[f]) ok_same = 0;
        in[0] = 0;
        fl = mk_call_veneer (in, out, mk_gv, flagsets[f]);
        if (out[0] != 0 || out[1] != in[1] * 2 + in[2] || out[5] != MAGIC || out[6] != 0 || fl != flagsets[f]) ok_zero = 0;
        for (int i = 2; i < 12; i++) if (i != 5 && i != 6 && i != 7 && out[i] != in[i]) ok_zero = 0;
        in[0] = 1;
        fl = mk_call_veneer (in, out, mk_gv, flagsets[f]);
        {
          const _kernel_oserror *er = (const _kernel_oserror *) out[0];
          if (er != &gv_error || (fl & 0x10000000u) == 0 || (fl & 0xE0000000u) != (flagsets[f] & 0xE0000000u) || out[1] != in[1] * 2 + in[2] || out[5] != MAGIC) ok_err = 0;
        }
      }
    check (ok_same, "generic veneer, handler changes nothing: r0 - r11 and the flags come back as they were (8 flag states)");
    check (ok_zero, "generic veneer, handler returns 0: r1 = r1*2+r2, r5 = the private word, the stack was 8 byte aligned, the flags as they were");
    check (ok_err, "generic veneer, handler returns an error: V set, N Z C as they were, r0 -> the error, the handler's changes come back");
  }
  /* the environment */
  { const char *v = getenv ("MK32$Path"); check (v != 0 && v[0] != 0, "getenv (\"MK32$Path\") finds the system variable"); }
  { void *p = malloc (1000), *q = malloc (5000); check (p != 0 && q != 0 && p != q && ((unsigned) p & 7) == 0, "malloc: two blocks from the RMA, 8 byte aligned"); free (p); free (q); }
  { char t[40]; snprintf (t, sizeof t, "%lld|%#x|% d|%.3d", -5000000000LL, 255u, 42, 7); check (!strcmp (t, "-5000000000|0xff| 42|007"), "snprintf of long long, #, space and precision"); }
  /* files in the scrap directory (OS_Find, OS_GBPB, OS_Args, OS_File 6 through stdio) */
  {
    const char *d = getenv ("Wimp$ScrapDir");
    char name[300], b[64];
    FILE *f;
    int ok = d != 0 && strlen (d) < 200;
    if (ok)
      {
        strcpy (name, d); strcat (name, ".MK32T");
        remove (name);
        f = fopen (name, "w");
        ok = f && fprintf (f, "hello %d\n", 42) == 9 && fputs ("second line\n", f) >= 0 && fclose (f) == 0;
        f = ok ? fopen (name, "r") : 0;
        ok = f && fgets (b, sizeof b, f) && !strcmp (b, "hello 42\n") && ftell (f) == 9 && fgets (b, sizeof b, f) && !strcmp (b, "second line\n") && fgetc (f) == EOF && feof (f);
        ok = ok && fseek (f, 6, SEEK_SET) == 0 && fread (b, 1, 2, f) == 2 && !memcmp (b, "42", 2) && fclose (f) == 0;
        f = ok ? fopen (name, "a") : 0;
        ok = f && fputs ("tail", f) >= 0 && fclose (f) == 0;
        f = ok ? fopen (name, "rb") : 0;
        ok = f && fseek (f, -4, SEEK_END) == 0 && fread (b, 1, 4, f) == 4 && !memcmp (b, "tail", 4) && ftell (f) == 25 && fclose (f) == 0;
        check (ok, "files in the scrap directory: write, read back, seek, append, read the tail");
        errno = 0;
        check (remove (name) == 0 && fopen (name, "r") == 0 && errno == ENOENT, "files: remove, and a file that is not there is ENOENT");
      }
    else check (0, "files: Wimp$ScrapDir is not set");
  }
  printf ("MK32 self test (%s): %d checks, %d FAILED\n", where, checks, fails);
  return fails;
}

static _kernel_oserror *do_after (const char *arg)
{
  unsigned cs = (unsigned) atoi (arg), t0, expected;
  _kernel_oserror *e;
  if (cs < 1 || cs > 100) return &gv_error;
  after_calls = after_bad_mode = after_irq_on = after_bad_sp = after_bad_pw = 0;
  e = _swix (0x3C, _INR (0, 2), cs, (unsigned) mk_after, MAGIC);   /* OS_CallEvery: R0 = centiseconds, R1 = the routine, R2 = its R12 */
  if (e) return e;
  t0 = monotonic ();
  while (monotonic () - t0 < 100) ;                                /* one second */
  _swix (0x3D, _INR (0, 1), (unsigned) mk_after, MAGIC);          /* OS_RemoveTickerEvent */
  expected = 100 / (cs + 1);                                      /* the R0 of OS_CallEvery is the interval minus 1 (the kernel adds 1: s/TickEvents), so 5 means every 6 centiseconds */
  printf ("MK32_After %u: %u calls in one second (about %u expected), in other than SVC mode %u, with IRQs on %u, with a bad stack %u, with a bad handle %u: %s\n",
          cs, after_calls, expected, after_bad_mode, after_irq_on, after_bad_sp, after_bad_pw,
          after_calls + 2 >= expected && after_calls <= expected + 2 && !after_bad_mode && !after_irq_on && !after_bad_sp && !after_bad_pw ? "ok" : "FAIL");
  return 0;
}

_kernel_oserror *mk_command (const char *arg_string, int argc, int number, void *pw)
{
  (void) argc; (void) pw;
  switch (number)
    {
    case CMD_MK32_Plain: printf ("MK32_Plain with %d arguments\n", argc); return 0;
    case CMD_MK32_Tokens: printf ("MK32_Tokens with %d arguments\n", argc); return 0;
    case CMD_MK32_Joined: printf ("MK32_Joined with %d arguments\n", argc); return 0;
    case CMD_MK32_Try:
      {
        _kernel_oserror *e = _swix (0x05, _IN (0), arg_string);
        if (e) printf ("MK32_Try: error &%X: %s\n", (unsigned) e->errnum, e->errmess); else printf ("MK32_Try: no error\n");
        return 0;
      }
    case CMD_MK32_SelfTest: selftest ("SVC mode"); return 0;
    case CMD_MK32_After: return do_after (arg_string);
    default: return 0;
    }
}

static void bye (void) { puts ("atexit: bye"); }

/* the keyboard: *RMRun MK32 keys asks for four lines and checks them (OS_ReadLine through stdin); run it by hand in a Task window */
static int keys_test (void)
{
  char line[100], w[40];
  int n = 0, bad = 0;
  fputs ("Type  alpha  and press Return: ", stdout);
  if (!fgets (line, sizeof line, stdin) || strcmp (line, "alpha\n")) { printf ("FAIL: got %s\n", feof (stdin) ? "the end of the input" : line); bad++; } else puts ("  ok");
  fputs ("Type  12 beta  and press Return: ", stdout);
  if (!fgets (line, sizeof line, stdin) || sscanf (line, "%d %39s", &n, w) != 2 || n != 12 || strcmp (w, "beta")) { printf ("FAIL: got %s\n", line); bad++; } else puts ("  ok");
  fputs ("Just press Return: ", stdout);
  if (!fgets (line, sizeof line, stdin) || strcmp (line, "\n")) { printf ("FAIL: got %s\n", line); bad++; } else puts ("  ok");
  fprintf (stdout, "Press Escape now: ");
  if (fgetc (stdin) != EOF || !feof (stdin)) { puts ("FAIL: Escape did not end the input"); bad++; } else puts ("  ok (the end of the input)");
  clearerr (stdin);
  fprintf (stderr, "stderr: %d problems\n", bad);
  return bad ? 1 : 0;
}
/* the end of a program with a file still open: the OS does not close files at OS_Exit, so the library does (exit): "leave" then "leavecheck" (two programs of the same module) show it */
static int leave_test (int check)
{
  static const char text[] = "left open at the end of the program\n";
  char name[200], line[100];
  const char *d = getenv ("Wimp$ScrapDir");
  FILE *f;
  if (!d) { puts ("FAIL: Wimp$ScrapDir is not set"); return 1; }
  strcpy (name, d); strcat (name, ".MK32L");
  if (!check)
    {
      f = fopen (name, "w");
      if (!f || fputs (text, f) < 0) { printf ("FAIL: the file could not be written (errno %d)\n", errno); return 1; }
      puts ("leave: wrote the file and left it open (no fclose); now run  *RMRun MK32 leavecheck");
      return 0;
    }
  f = fopen (name, "r");
  if (!f) { printf ("FAIL: the file could not be opened (errno %d): it was left open or its text was lost\n", errno); return 1; }
  if (!fgets (line, sizeof line, f) || strcmp (line, text)) { puts ("FAIL: the file does not have what the program wrote"); fclose (f); return 1; }
  fclose (f);
  if (remove (name)) { printf ("FAIL: remove (errno %d)\n", errno); return 1; }
  puts ("  ok: the file was flushed and closed at the end of the previous program");
  return 0;
}
int main (int argc, char **argv)
{
  int i;
  printf ("MK32 as a program (user mode): argc=%d\n", argc);
  for (i = 0; i < argc; i++) printf ("argv[%d]=<%s>\n", i, argv[i]);
  atexit (bye);
  if (argc > 1 && !strcmp (argv[1], "test")) return selftest ("USER mode") ? 1 : 0;
  if (argc > 1 && !strcmp (argv[1], "keys")) return keys_test ();
  if (argc > 1 && !strcmp (argv[1], "leave")) return leave_test (0);
  if (argc > 1 && !strcmp (argv[1], "leavecheck")) return leave_test (1);
  if (argc > 1 && !strcmp (argv[1], "exit")) exit (argc > 2 ? atoi (argv[2]) : 7);
  return 0;
}
