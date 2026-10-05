/* wrapexec.c -- a stand-in for cc1 and as that LOGS what the gcc driver does when it starts a compiler pass, and (in the mode "run") then runs the real program.  Installed as wrap/cc1 and wrap/as,
   found by  gcc -Bwrap/ ... .  Everything goes to the file Wrap$Log (default "wraplog", appended): the arguments (and the contents of an @file argument), the file descriptors 0 - 9 (open? a tty? the mode),
   the result of a write to descriptors 1 and 2, the variables the compilers read, whether the file after -o can be opened for writing.
   Wrap$Mode "log" (the default): log and end with status 0, no real program is started.  Wrap$Mode "run": the real program (the variable Wrap$cc1 or Wrap$as holds its name) is started with its stderr
   redirected to <log>.err, waited for; how it ended is logged, what it wrote to stderr is logged AND written to this program's stderr, and this program ends with the same status.
   Wrap$Mode "fail": log and end with status 1.   */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static char *getenv_dup (const char *name)
{
  const char *v = getenv (name);
  return v ? strdup (v) : NULL;
}

static void show_var (FILE *log, const char *name)
{
  const char *v = getenv (name);
  fprintf (log, "  %-22s %s%s%s\n", name, v ? "\"" : "(not set)", v ? v : "", v ? "\"" : "");
}

int main (int argc, char **argv)
{
  const char *slash = strrchr (argv[0], '/');
  const char *name = slash ? slash + 1 : argv[0];
  char var[64];
  snprintf (var, sizeof var, "Wrap$%s", name);
  /* UnixLib's getenv ("...$...") returns ONE static buffer that the next such call frees: copy every result at once (wrapexec 1.0 did not, and read garbage) */
  char *real = getenv_dup (var);
  char *logname = getenv_dup ("Wrap$Log");
  char *mode = getenv_dup ("Wrap$Mode");
  if (!logname) logname = "wraplog";
  if (!mode) mode = "log";
  FILE *log = fopen (logname, "a");
  if (!log)
    {
      fprintf (stderr, "wrapexec: cannot open the log %s: %s\n", logname, strerror (errno));
      return 111;
    }
  fprintf (log, "==== %s started (wrapexec 1.2, mode %s): pid %d, ppid %d, argc %d\n", name, mode, (int) getpid (), (int) getppid (), argc);
  for (int i = 0; i < argc; i++) fprintf (log, "  argv[%d] = \"%s\"\n", i, argv[i]);
  if (argc == 2 && argv[1][0] == '@')
    {
      FILE *r = fopen (argv[1] + 1, "r");
      if (!r) fprintf (log, "  the response file %s cannot be opened: %s\n", argv[1] + 1, strerror (errno));
      else
	{
	  char line[600];
	  fprintf (log, "  the response file %s:\n", argv[1] + 1);
	  int n = 0;
	  while (fgets (line, sizeof line, r) && n++ < 60) fprintf (log, "    | %s", line);
	  fclose (r);
	}
    }
  for (int fd = 0; fd <= 9; fd++)
    {
      struct stat st;
      errno = 0;
      int r = fstat (fd, &st);
      fprintf (log, "  fd %d: %s, mode %o, isatty %d, fcntl flags %d\n", fd, r == 0 ? "open" : "NOT OPEN", r == 0 ? (unsigned) st.st_mode : 0u, isatty (fd), fcntl (fd, F_GETFD));
    }
  const char *hello1 = "wrapexec: a line to stdout\n";
  const char *hello2 = "wrapexec: a line to stderr (if you can read this on the screen, the stderr that the driver gave this program works)\n";
  errno = 0;
  ssize_t w1 = write (1, hello1, strlen (hello1));
  fprintf (log, "  write to fd 1 returned %d (errno %d)\n", (int) w1, errno);
  errno = 0;
  ssize_t w2 = write (2, hello2, strlen (hello2));
  fprintf (log, "  write to fd 2 returned %d (errno %d: %s)\n", (int) w2, errno, errno ? strerror (errno) : "-");
  errno = 0;
  int r3 = fputs ("wrapexec: a line to stderr through stdio\n", stderr);
  fflush (stderr);
  fprintf (log, "  fputs to stderr returned %d, ferror %d\n", r3, ferror (stderr));
  fprintf (log, "  the variables the compilers read:\n");
  const char *vars[] = { "TMPDIR", "Sys$RCLimit", "Wimp$ScrapDir", "COLLECT_GCC", "COLLECT_GCC_OPTIONS", "COMPILER_PATH", "LIBRARY_PATH", "GCC_EXEC_PREFIX", "UnixEnv$cc1$sfix", "UnixEnv$as$sfix",
			 "cc1$Heap", "cc1$HeapMax", "as$Heap", "as$HeapMax", "THROWBACK_DEBUG", NULL };
  for (int i = 0; vars[i]; i++) show_var (log, vars[i]);
  for (int i = 1; i + 1 < argc; i++)
    if (!strcmp (argv[i], "-o"))
      {
	errno = 0;
	int fd = open (argv[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0666);
	fprintf (log, "  open of the output file \"%s\" for writing: %s (errno %d: %s)\n", argv[i + 1], fd >= 0 ? "ok" : "FAILED", errno, errno ? strerror (errno) : "-");
	if (fd >= 0) close (fd);
      }
  fflush (log);
  if (!strcmp (mode, "log"))
    {
      fprintf (log, "  mode log: no real program, ends with status 0\n");
      fclose (log);
      return 0;
    }
  if (!strcmp (mode, "fail"))
    {
      fprintf (log, "  mode fail: no real program, ends with status 1\n");
      fclose (log);
      return 1;
    }
  if (!real)
    {
      fprintf (log, "  the variable %s is not set: the real program is not known\n", var);
      fclose (log);
      return 112;
    }
  fprintf (log, "  the real program: %s\n", real);
  fflush (log);
  char errname[400];
  snprintf (errname, sizeof errname, "%s.err", logname);
  unlink (errname);
  char **nargv = malloc ((argc + 1) * sizeof *nargv);
  for (int i = 0; i < argc; i++) nargv[i] = argv[i];
  nargv[argc] = NULL;
  pid_t pid = vfork ();
  if (pid == 0)
    {
      int fd = open (errname, O_WRONLY | O_CREAT | O_APPEND, 0666);
      if (fd >= 0) dup2 (fd, 2);
      execv (real, nargv);
      _exit (127);
    }
  if (pid < 0)
    {
      fprintf (log, "  vfork failed: %s\n", strerror (errno));
      fclose (log);
      return 113;
    }
  int st = 0;
  pid_t r = waitpid (pid, &st, 0);
  int status = (r == pid && WIFEXITED (st)) ? WEXITSTATUS (st) : 200;
  if (r != pid) fprintf (log, "  waitpid failed: pid %d, errno %d\n", (int) r, errno);
  else if (WIFEXITED (st)) fprintf (log, "  the real program ended with exit status %d\n", status);
  else fprintf (log, "  the real program was killed by signal %d\n", WTERMSIG (st));
  FILE *e = fopen (errname, "r");
  if (e)
    {
      char line[512];
      fprintf (log, "  what the real program wrote to its stderr:\n");
      int any = 0;
      while (fgets (line, sizeof line, e))
	{
	  any = 1;
	  fprintf (log, "    | %s", line);
	  fputs (line, stderr);
	}
      if (!any) fprintf (log, "    (nothing)\n");
      fclose (e);
    }
  else fprintf (log, "  there is no %s: the real program wrote nothing to its stderr, or the file could not be made\n", errname);
  fprintf (log, "  wrapexec ends with status %d\n", status);
  fclose (log);
  return status;
}
