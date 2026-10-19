/* ids.c - who the process is: user and group ids, uname, gethostname, umask (RISC OS has no users: the ids are fixed, as in UnixLib 64's defaults; UnixLib has uid 1 unless UnixEnv$uid says otherwise).  The SDK of RISC OS 64. */
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

uid_t getuid (void) { return 1; }
uid_t geteuid (void) { return 1; }
gid_t getgid (void) { return 1; }
gid_t getegid (void) { return 1; }
pid_t getppid (void) { const char *p = getenv ("UnixEnv$ppid"); return p && *p ? (pid_t) atol (p) : 0; }	/* set by system (), popen () and posix_spawn () for the program they start: the process that started this one */

int gethostname (char *name, size_t len)
{
  const char *h = getenv ("Inet$HostName");
  if (!h || !*h) h = "riscos";
  strncpy (name, h, len);
  if (len) name[len - 1] = 0;
  return 0;
}

int uname (struct utsname *u)
{
  memset (u, 0, sizeof *u);
  strcpy (u->sysname, "RISC OS");
  gethostname (u->nodename, sizeof u->nodename);
  strcpy (u->release, "64");
  strcpy (u->version, "RISC OS 64");
  strcpy (u->machine, "aarch64");
  return 0;
}

mode_t umask (mode_t mask)
{
  static mode_t current = 022;
  mode_t old = current;
  current = mask & 0777;
  return old;
}
