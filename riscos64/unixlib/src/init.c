/* init.c - the start-up of UnixLib 64: the runtime features that the system variables UnixEnv$<program>$<feature> and UnixEnv$<feature> switch (libunixlib/unix/features.c of UnixLib):
   "nonametrans" (no file name translation) and "sfix" (the suffixes that are swapped, default c:cc:f:h:i:ii:l:o:p:s:y), and the special directories (UnixFS$/<name>) of __riscosify.
   riscos_start.c of the SDK calls __ul64_init (argc, argv) before main () if this file is linked in.  Revised BSD licence (licenses/UnixLib-COPYING.txt).  */
#include "ul64.h"
#include <dirent.h>
#include <sys/statvfs.h>
#include <unistd.h>

/* References to the functions that libriscosstub.a also has as stubs (ENOSYS): code in libriscosstub.a that is linked in later (glob) needs these, and the stubs would be taken for it if UnixLib's were not
   already in the link.  This file is always linked in with -munixlib (-u __ul64_init), so these are UnixLib's. */
const void *const __ul64_exports[] = { (const void *) opendir, (const void *) readdir, (const void *) closedir, (const void *) statvfs, (const void *) chdir, (const void *) getcwd, (const void *) rmdir };

static const char sfix_default[] = "c:cc:f:h:i:ii:l:o:p:s:y";

/* the value of the system variable, or NULL (OS_ReadVarVal, type string) */
static char *os_var (const char *name, char *buf, size_t size)
{
  long r[10] = { (long) name, (long) buf, (long) size - 1, 0, 0 };
  if (__os_swi (OS_ReadVarVal, r) || r[2] < 0) return NULL;
  buf[r[2]] = 0;
  return buf;
}

/* UnixEnv$<program>$<variable>, else UnixEnv$<variable> */
static char *feature (const char *program, const char *variable, char *buf, size_t size)
{
  char name[256];
  char *r = NULL;
  if (program && *program && strlen (program) + strlen (variable) < sizeof name - 12)
    {
      snprintf (name, sizeof name, "UnixEnv$%s$%s", program, variable);
      r = os_var (name, buf, size);
    }
  if (!r) { snprintf (name, sizeof name, "UnixEnv$%s", variable); r = os_var (name, buf, size); }
  return r;
}

/* argv[0] as UnixLib gives it: the full path of the program in Unix form (OS_FSControl 37 canonicalises it, looking along Run$Path), "/ADFS::4.$/bin/hello" */
static void unixify_argv0 (char **argv)
{
  char ros[MAXPATHLEN];
  long r[10] = { 37, (long) argv[0], (long) ros, (long) "Run$Path", 0, sizeof ros };
  char *u = NULL;
  if (!__os_swi (0x29, r)) u = __unixify_std (ros, NULL, 0, __RISCOSIFY_FILETYPE_NOTSPECIFIED);		/* OS_FSControl */
  if (!u) u = __unixify_std (argv[0], NULL, 0, __RISCOSIFY_FILETYPE_NOTSPECIFIED);
  if (u) argv[0] = u;
}

void __ul64_init (int argc, char **argv)
{
  char buf[256], program[128];
  const char *leaf = (argc > 0 && argv && argv[0]) ? argv[0] : "";
  const char *p;
  char *v;
  /* the leaf name of the program: after the last '.' or ':' (RISC OS) or '/' (Unix) */
  for (p = leaf; *p; p++) if (*p == '.' || *p == ':' || *p == '/') leaf = p + 1;
  strncpy (program, leaf, sizeof program - 1); program[sizeof program - 1] = 0;
  __sdirinit ();
  v = feature (NULL, "nonametrans", buf, sizeof buf);
  if (!v) v = feature (program, "nonametrans", buf, sizeof buf);
  if (v) __set_riscosify_control (__RISCOSIFY_NO_PROCESS);
  v = feature (program, "sfix", buf, sizeof buf);
  __sfixinit (v ? v : sfix_default);
  if (argc > 0 && argv && argv[0] && !(__get_riscosify_control () & __RISCOSIFY_NO_PROCESS)) unixify_argv0 (argv);
}
