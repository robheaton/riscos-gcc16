/* OSLib functions in a module through libOSLib32.a: the X form returns the error, the non-X form returns the result register and raises the error (OS_GenerateError). */
#include <stdio.h>
#include <string.h>
#include "kernel.h"
#include "oslib/os.h"
#include "header.h"

_kernel_oserror *ol_command (const char *arg_string, int argc, int number, void *pw)
{
  char b[64]; int used, ctx; os_error *e;
  (void) arg_string; (void) argc; (void) number; (void) pw;
  e = xos_read_var_val ("OlT$Var", b, sizeof b, 0, os_VARTYPE_STRING, &used, &ctx, 0);
  printf ("X found: %s used %d\n", e ? "error" : "ok", used);
  if (!e) { b[used] = 0; printf ("  value '%s'\n", b); }
  e = xos_read_var_val ("OlT$Missing", b, sizeof b, 0, os_VARTYPE_STRING, &used, &ctx, 0);
  printf ("X missing: error %s\n", e ? e->errmess : "none");
  int c = os_read_var_val ("OlT$Var", b, sizeof b, 0, os_VARTYPE_STRING, &used, 0);
  printf ("non-X found: context %d used %d\n", c, used);
  unsigned t1 = os_read_monotonic_time ();
  unsigned t2 = os_read_monotonic_time ();
  printf ("monotonic: %u %u\n", t1, t2);
  os_write0 ("write0 works");
  os_new_line ();
  os_read_var_val ("OlT$Missing", b, sizeof b, 0, os_VARTYPE_STRING, &used, 0);
  printf ("after the non-X error\n");
  return 0;
}

_kernel_oserror *ol_final (int fatal, int podule, void *pw)
{
  (void) fatal; (void) podule; (void) pw;
  return 0;
}
