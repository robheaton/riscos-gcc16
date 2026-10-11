/* errmod.c - the module ErrMod of pack/module47: CMunge's error-identifiers on the machine (the numbers and messages of the error blocks, an error returned by a command, _kernel_raise_error) and, in
   *ErrMod_Test, system (), some _kernel_ functions and the wide character functions of 16.2.0-20. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <errno.h>
#include <kernel.h>
#include <swis.h>
#include "header.h"

static int checks, fails;
static void check (int ok, const char *what)
{
  checks++;
  if (!ok) fails++;
  printf ("  %s  %s\n", ok ? "ok  " : "FAIL", what);
}

_kernel_oserror *em_final (int fatal, int podule, void *pw) { (void) fatal; (void) podule; (void) pw; return 0; }

static const _kernel_oserror *by_number (int n)
{
  switch (n)
    {
    case 0: return err_first;
    case 1: return err_second;
    case 2: return err_numbered;
    default: return err_next;
    }
}

static void test (void)
{
  char b[64];
  int used = 0;
  _kernel_swi_regs r;
  wchar_t w[8];
  mbstate_t st;
  printf ("ErrMod_Test\n");
  check (ERROR_BASE == 0x838000, "ERROR_BASE is 0x838000");
  check (err_first->errnum == 0x838000 && !strcmp (err_first->errmess, "The first error of ErrMod"), "err_first: &838000, its message");
  check (err_second->errnum == 0x838001 && !strcmp (err_second->errmess, "The second error: a message with a number 2"), "err_second: &838001, its message");
  check (err_numbered->errnum == 0x838010 && !strcmp (err_numbered->errmess, "The third error has a number of its own"), "err_numbered: the number it was given");
  check (err_next->errnum == 0x838002, "err_next: the number after err_second (a number of its own does not count)");
  check (((unsigned) err_first & 3) == 0 && ((unsigned) err_next & 3) == 0, "the error blocks are word aligned");
  /* system (): the command line interpreter */
  check (system (NULL) != 0, "system (NULL): there is a command processor");
  r.r[0] = (int) "ErrMod$Var"; r.r[1] = (int) b; r.r[2] = sizeof b; r.r[3] = 0; r.r[4] = 3;
  _kernel_swi (OS_ReadVarVal | 0x20000, &r, &r);                                    /* (gone, or an old value: the next calls make it) */
  check (system ("Set ErrMod$Var from-system") == 0, "system (\"Set ErrMod$Var from-system\") is 0");
  r.r[0] = (int) "ErrMod$Var"; r.r[1] = (int) b; r.r[2] = sizeof b; r.r[3] = 0; r.r[4] = 0;
  check (_kernel_swi (OS_ReadVarVal | 0x20000, &r, &r) == NULL && (used = r.r[2]) == 11 && !memcmp (b, "from-system", 11), "the command ran: ErrMod$Var is \"from-system\"");
  system ("Unset ErrMod$Var");
  errno = 0;
  check (system ("NoSuchCommandAtAll") == -1 && errno == EIO, "system () of a command that does not exist: -1 and EIO");
  check (_kernel_last_oserror () != NULL, "the OS error stays in _kernel_last_oserror ()");
  check (_kernel_system ("Set ErrMod$Var2 x", 0) == 0 && _kernel_system ("NoSuchCommandAtAll", 0) == -2, "_kernel_system: 0, and -2 for an error");
  system ("Unset ErrMod$Var2");
  check (_kernel_escape_seen () == 0, "_kernel_escape_seen () is 0 (nobody pressed Escape)");
  check (_kernel_fpavailable () == 0 && _kernel_command_string () == NULL, "_kernel_fpavailable () is 0, _kernel_command_string () is NULL");
  /* the wide character functions */
  check (wcslen (L"wide") == 4 && wcscmp (L"abc", L"abd") < 0 && wcsncmp (L"abc", L"abd", 2) == 0, "wcslen, wcscmp, wcsncmp");
  wcscpy (w, L"abc"); wcscat (w, L"def");
  check (wcscmp (w, L"abcdef") == 0 && wcschr (w, L'd') == w + 3 && wcsstr (w, L"cde") == w + 2, "wcscpy, wcscat, wcschr, wcsstr");
  check (iswalpha (L'q') && !iswalpha (L'7') && iswdigit (L'7') && towupper (L'q') == L'Q' && towlower (L'Q') == L'q' && iswspace (L' ') && !iswalpha (0xE9), "iswalpha, iswdigit, towupper, towlower, iswspace (ASCII only)");
  memset (&st, 0, sizeof st);
  check (mbrtowc (w, "A", 1, &st) == 1 && w[0] == L'A' && wcrtomb (b, L'z', &st) == 1 && b[0] == 'z', "mbrtowc and wcrtomb");
  errno = 0;
  check (mbrtowc (w, "\xE9", 1, &st) == (size_t) -1 && errno == EILSEQ, "a byte above 127 is EILSEQ (the C locale)");
  check (mblen ("x", 1) == 1 && mblen ("", 1) == 0 && wcstol (L"  -42xyz", NULL, 10) == -42 && wcstod (L"2.5", NULL) == 2.5, "mblen, wcstol, wcstod");
  printf ("ErrMod_Test: %d checks, %d FAILED\n", checks, fails);
  (void) used;
}

_kernel_oserror *em_table (const char *arg_string, int argc, int number, void *pw)
{
  (void) pw; (void) argc;
  switch (number)
    {
    case CMD_ErrMod_Test:
      test ();
      return 0;
    case CMD_ErrMod_Fail:
      return (_kernel_oserror *) by_number (atoi (arg_string));
    case CMD_ErrMod_Try:
      {
        int r = _kernel_oscli (arg_string);
        const _kernel_oserror *e = _kernel_last_oserror ();
        if (r == _kernel_ERROR && e) printf ("ErrMod_Try: the command gave error &%X \"%s\"\n", (unsigned) e->errnum, e->errmess);
        else printf ("ErrMod_Try: the command gave no error\n");
        return 0;
      }
    default:
      _kernel_raise_error (err_second);
      printf ("AFTER: _kernel_raise_error came back (a real kernel should not get here)\n");
      return 0;
    }
}
