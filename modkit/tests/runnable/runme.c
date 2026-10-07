/* runme.c - main () of RunMe: prints its arguments, runs an atexit function, and ends with a code that says what happened:
     RunMe                       argc = 1, exit code 10
     RunMe a "b c" d             argc = 4, exit code 40
     RunMe exit [n]              exit (n) from main (default 7)
     RunMe return [n]            return n from main (default 3)
     RunMe abort                 abort ()
   The module commands run in SVC mode, where there is no program: *RunMeExit [n] calls exit (n), which must be an error, not the end of the desktop. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "header.h"

static int started;
static void bye (void) { puts ("atexit: bye"); }

_kernel_oserror *runme_init (const char *tail, int podule_base, void *pw)
{
  (void) tail; (void) podule_base; (void) pw;
  started = 1;
  return 0;
}
_kernel_oserror *runme_final (int fatal, int podule_base, void *pw) { (void) fatal; (void) podule_base; (void) pw; return 0; }
_kernel_oserror *runme_command (const char *arg_string, int argc, int number, void *pw)
{
  (void) pw;
  if (number == CMD_RunMeHello) { printf ("hello from RunMe (%d)\n", started); return 0; }
  if (number == CMD_RunMeExit) { atexit (bye); exit (argc ? atoi (arg_string) : 5); }
  return 0;
}

int main (int argc, char **argv)
{
  int i;
  printf ("argc=%d\n", argc);
  for (i = 0; i < argc; i++) printf ("argv[%d]=<%s> (%d)\n", i, argv[i], (int) strlen (argv[i]));
  printf ("argv[argc]=%s\n", argv[argc] ? "not null" : "null");
  atexit (bye);
  if (argc > 1 && !strcmp (argv[1], "exit")) exit (argc > 2 ? atoi (argv[2]) : 7);
  if (argc > 1 && !strcmp (argv[1], "return")) return argc > 2 ? atoi (argv[2]) : 3;
  if (argc > 1 && !strcmp (argv[1], "abort")) abort ();
  return argc * 10;
}
