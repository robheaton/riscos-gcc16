/* argtest.c 1.2 -- prints what the program was started with: argc, every argument in quotes, the raw OS_GetEnv command string (UnixLib has kept it), the total length of all the arguments, and the size of the DDEUtils
   command line buffer now (UnixLib has already taken and cleared it at start-up: a stale extended command line would show as extra arguments).  Exit status: the number in argv[1] if there is one and it is a number, else 0.   */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <swis.h>
int main (int argc, char **argv)
{
  size_t total = 0;
  printf ("argtest 1.2: argc=%d\n", argc);
  for (int i = 0; i < argc; i++)
    {
      total += strlen (argv[i]) + (i ? 1 : 0);
      printf ("  argv[%d] = \"%s\"\n", i, argv[i]);
    }
  printf ("  the length of the arguments after the program name, with one space each: %lu\n", (unsigned long) total - strlen (argv[0]));
  const char *env = NULL;
  _kernel_oserror *e0 = _swix (0x10 /* OS_GetEnv */, _OUT (0), &env);
  if (e0) printf ("  OS_GetEnv failed: %s\n", e0->errmess);
  else printf ("  OS_GetEnv command string (length %lu): [%s]\n", (unsigned long) strlen (env), env);
  int n = -1;
  _kernel_oserror *e = _swix (0x42583 /* DDEUtils_GetCLSize */, _OUT (0), &n);
  printf ("  DDEUtils command line buffer now: %s\n", e ? e->errmess : (n == 0 ? "empty" : "NOT EMPTY"));
  /* when every argument has the form wNN (the words of the tests) in order: are they all there? */
  int words = 0, ok = 1;
  for (int i = 1; i < argc; i++)
    {
      int k = 0;
      if (argv[i][0] == 'w' && sscanf (argv[i] + 1, "%d", &k) == 1) { words++; if (k != i) ok = 0; }
    }
  if (words && words == argc - 1) printf ("  the arguments are the words w01 ... w%02d%s\n", words, ok ? ": ALL ARRIVED IN ORDER" : ": BUT NOT ALL, OR NOT IN ORDER");
  fflush (stdout);
  if (words && words == argc - 1) return ok ? 0 : 1;
  return (argc > 1 && argv[1][0] >= '0' && argv[1][0] <= '9') ? atoi (argv[1]) : 0;
}
