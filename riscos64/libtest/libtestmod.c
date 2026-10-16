/* libtestmod.c - the module LibTest64: the C library of the kit (modkit/lib, compiled for AArch64) and its test program, libtest.c, inside a module.  *LibTest64 runs the test on a stack of its own (the
   module's stack is the SVC stack, 32 KB that the whole OS shares).  Which sections run: the system variable LT_ONLY (*Set LT_ONLY ctype,string), all if it is not set. */
#include "kernel.h"
#include "libtestmodh.h"

extern int libtest_main (int argc, char **argv);
extern long a64_call_on_stack (long (*fn) (void), void *top);			/* libtestmod.S */
#define STACK_BYTES (384 * 1024)
static char *stack;

_kernel_oserror *lt_init (const char *tail, int podule_base, void *pw)
{
  stack = mk_rma_claim (STACK_BYTES);
  if (!stack) return mk_error (0x20C01, "LibTest64: no room for the stack");
  return 0;
}

_kernel_oserror *lt_final (int fatal, int podule_base, void *pw)
{
  if (stack) mk_rma_free (stack);
  stack = 0;
  return 0;
}

static long run (void)
{
  static char name[] = "libtest";
  char *argv[2] = { name, 0 };
  return libtest_main (1, argv);
}

_kernel_oserror *lt_command (const char *arg_string, int argc, int number, void *pw)
{
  a64_call_on_stack (run, stack + STACK_BYTES);
  return 0;
}
