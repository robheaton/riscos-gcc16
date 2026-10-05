/* vtest.c 1.1 -- vfork + exec + wait, the way the compiler driver starts cc1 and as:  vtest N PROGRAM [ARGUMENTS...]  starts PROGRAM N times, one after the other, and prints how each ended and how long it took.
   vtest N -w K PROGRAM [ARGUMENTS...] appends K generated words (w001 w002 ... 4 characters each) to the arguments, to try command lines of any length (UnixLib's exec uses the DDEUtils module from 1024 characters).
   (seqtest of the unixlib packs also runs daprobe after every child; this does not.)  Every line is flushed before the next step, so that a hang shows where it was.  */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static __attribute__ ((noinline)) int run (const char *prog, char *const av[], int *how)
{
  pid_t pid = vfork ();
  if (pid == 0)
    {
      execv (prog, av);
      _exit (127);					/* the child shares our memory and stack until it execs: nothing else may happen here */
    }
  if (pid < 0)
    {
      *how = errno;
      return -1;
    }
  int st = 0;
  pid_t r = waitpid (pid, &st, 0);
  if (r != pid)
    {
      *how = errno;
      return -3;
    }
  if (WIFEXITED (st))
    return WEXITSTATUS (st);
  *how = WTERMSIG (st);
  return -2;
}

int main (int argc, char **argv)
{
  if (argc < 3)
    {
      fprintf (stderr, "usage: vtest N [-w K] program [arguments]\n");
      return 2;
    }
  int n = atoi (argv[1]);
  int words = 0;
  int first = 2;
  if (argc > 4 && !strcmp (argv[2], "-w"))
    {
      words = atoi (argv[3]);
      first = 4;
    }
  int nargs = argc - first;
  char **av = malloc ((nargs + words + 1) * sizeof *av);
  for (int i = 0; i < nargs; i++) av[i] = argv[first + i];
  size_t total = 0;
  for (int i = 0; i < words; i++)
    {
      char *w = malloc (16);
      snprintf (w, 16, "w%03d", i + 1);
      av[nargs + i] = w;
    }
  av[nargs + words] = NULL;
  for (int i = 0; i < nargs + words; i++) total += strlen (av[i]) + 1;
  printf ("vtest 1.1 (fix level %ld): %d children of \"%s\" with %d arguments (%lu characters of command line), one after the other\n", sysconf (0x4700), n, av[0], nargs + words - 1, (unsigned long) total);
  fflush (stdout);
  for (int i = 1; i <= n; i++)
    {
      printf ("--- child %d of %d: vfork + exec ...\n", i, n);
      fflush (stdout);
      clock_t t0 = clock ();
      int how = 0;
      int r = run (av[0], av, &how);
      clock_t t1 = clock ();
      printf ("--- child %d returned %d%s (errno / signal %d) after %ld cs\n", i, r, r == -1 ? " = vfork failed" : r == -2 ? " = killed by a signal" : r == -3 ? " = waitpid failed" : r == 127 ? " = exec failed or the child said 127" : "", how, (long) (t1 - t0));
      fflush (stdout);
    }
  printf ("vtest: done\n");
  return 0;
}
