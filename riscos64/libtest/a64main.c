/* a64main.c - the main of libtest64: the command line of the program is
       libtest64 [-v] [-only=name,name,...]
   -only=  runs only those sections (LT_ONLY of the test: a system variable on the machine, here made from the command line: the library's getenv reads system variables, and a64hooks.c answers for these two);
   -v      prints every result (the test reads the argument itself).
   libtest.c is compiled with  -Dmain=libtest_main. */
#include <string.h>

extern int libtest_main (int argc, char **argv);

extern const char *a64_only, *a64_verbose;                                  /* a64hooks.c: the system variables LT_ONLY and LT_VERBOSE that the library's getenv reads */

int main (int argc, char **argv)
{
  int i;
  for (i = 1; i < argc; i++)
    {
      if (!strncmp (argv[i], "-only=", 6)) a64_only = argv[i] + 6;
      else if (!strcmp (argv[i], "-v")) a64_verbose = "1";
    }
  libtest_main (argc, argv);                                                 /* (failures are the FAIL lines and the TOTAL line; the return code is 0 so that an Obey file goes on to the next section) */
  return 0;
}
