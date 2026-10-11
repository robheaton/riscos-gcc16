/* modparity.c - what 16.2.0-20 added for modules: the error blocks of error-identifiers:, wide characters, system (), long double maths and the _kernel_ functions.
   Compiles with the headers of the kit and links with nothing undefined. */
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <math.h>
#include <kernel.h>
#include "modparity.h"

_kernel_oserror *modparity_cmd (const char *args, int argc, int number, void *pw)
{
  wchar_t w[8];
  char c[8];
  long double x = 2.0L;
  (void) args; (void) argc; (void) number; (void) pw;
  if (mbstowcs (w, "ab", 8) != 2 || wcslen (w) != 2 || !iswalpha (w[0]) || towupper (w[0]) != L'A') return err_one;
  if (wcstombs (c, w, 8) != 2 || strcmp (c, "ab")) return err_two;
  if (sqrtl (x * x) != x || __modlib_stack_left () <= 0) return err_one;
  if (_kernel_fpavailable ()) return err_two;
  system ("Echo ModParity");
  return NULL;
}
