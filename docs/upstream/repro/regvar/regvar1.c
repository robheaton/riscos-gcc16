/* regvar1.c - the result of an asm in a LOCAL REGISTER VARIABLE, read after a call.

   This is the shape of UnixLib's incl-local/internal/os.h wrapper SWI_DDEUtils_GetCLSize () and of its use in unix/unix.c (__unixinit), with the SWI
   replaced by two instructions that need no RISC OS ("SWI DDEUtils_GetCLSize" -> "mov r0, #7": the size 7 in r0, and no error), so that only the
   compiler decides the outcome.  f () returns  strlen (s) + arg_size  with arg_size = 7.

   Build for RISC OS (or any ARM Linux target) and run it:    gcc -O2 -o regvar1 regvar1.c ; ./regvar1
   or only look at the code:                                  gcc -O2 -S -o - regvar1.c     (in f: is the result of the asm taken from r0 before or after "bl strlen"?)
   GCC 4.7.4 and 10.2.0:  f ("hello") = 12.    GCC 16.2.0:  f ("hello") = 10 (arg_size = the result of strlen: r0 is read after the call).
   -DFIXED builds the rewritten wrapper of patches/unixlib-inline-swi-register-variables.patch: the result is copied out INSIDE the asm, into an ordinary
   variable; 12 with every compiler.   -DNO_MAIN leaves out main () (verify/tools/sim-regvar.py runs f () on an interpreter).  */
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int errnum; char errmess[252]; } oserror;

#ifndef FIXED
/* as in os.h, but the asm does not make a SWI */
static __inline__ const oserror * __attribute__ ((always_inline))
SWI_Fake_GetCLSize (size_t *__len)
{
  register size_t len __asm ("r0");
  register const oserror *err;
  __asm__ volatile ("mov\tr0, #7\n\t"
		    "mov\t%[err], #0\n\t"
		    : [err] "=r" (err), "=r" (len)
		    :
		    : "r14", "cc");
  if (__len && !err)
    *__len = len;
  return err;
}
#else
/* as in the patched os.h */
static __inline__ const oserror * __attribute__ ((always_inline))
SWI_Fake_GetCLSize (size_t *__len)
{
  size_t len;
  register const oserror *err;
  __asm__ volatile ("mov\tr0, #7\n\t"
		    "mov\t%[err], #0\n\t"
		    "mov\t%[len], r0\n\t"
		    : [len] "=&r" (len), [err] "=&r" (err)
		    :
		    : "r0", "r14", "cc");
  if (__len && !err)
    *__len = len;
  return err;
}
#endif

char *sink;				/* so that the string is used */
extern const oserror *fill (char *, size_t);

size_t __attribute__ ((noinline))
f (const char *cli_in)
{
  size_t arg_size;
  if (SWI_Fake_GetCLSize (&arg_size) != NULL)
    arg_size = 0;

  size_t com_size = strlen (cli_in);
  size_t cli_size = com_size + arg_size;
  char *cli = malloc (cli_size + 2);
  if (cli != NULL)
    {
      memcpy (cli, cli_in, com_size);
      if (arg_size != 0)
	{
	  cli[com_size] = ' ';
	  if (fill (cli + com_size + 1, arg_size) != NULL)
	    abort ();
	  cli[com_size + 1 + arg_size] = '\0';
	}
      else
	cli[com_size] = '\0';
    }
  sink = cli;
  return cli_size;
}

const oserror *
fill (char *p, size_t n)
{
  memset (p, 'x', n);
  return NULL;
}

#ifndef NO_MAIN
int
main (int argc, char **argv)
{
  size_t want = strlen (argv[0]) + 7, got = f (argv[0]);
  printf ("regvar1: f (\"%s\") = %lu, expected %lu: %s\n", argv[0], (unsigned long) got, (unsigned long) want,
	  got == want ? "ok" : "WRONG (the result of the asm was read from r0 after the call of strlen)");
  return got != want;
}
#endif
