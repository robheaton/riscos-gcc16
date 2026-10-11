/* dlmod.c - a module that calls DeskLib32 (the module flavour, libDesk-sclmodule.a, linked with the C library of modkit): *DLMod_Test runs the checks of dltest.c in SVC mode. */
#include <kernel.h>
#include "dlmod.h"

extern int dltest_run (void);

_kernel_oserror *dlmod_cmd (const char *args, int argc, int number, void *pw)
{
  (void) args; (void) argc; (void) number; (void) pw;
  dltest_run ();
  return 0;
}
