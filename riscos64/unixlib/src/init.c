/* init.c - the start-up of UnixLib 64: the runtime features that the system variables UnixEnv$<program>$<feature> and UnixEnv$<feature> switch (libunixlib/unix/features.c of UnixLib):
   "nonametrans" (no file name translation) and "sfix" (the suffixes that are swapped, default c:cc:f:h:i:ii:l:o:p:s:y), and the special directories (UnixFS$/<name>) of __riscosify.
   riscos_start.c of the SDK calls __ul64_init (argc, argv) before main () if this file is linked in.  Revised BSD licence (licenses/UnixLib-COPYING.txt).  */
#include "ul64.h"

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
}
