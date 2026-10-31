/* ulrun.c - the launcher of the sample scripts in the native compiler packages for RISC OS 64 (riscos64-native, riscos64-native32).

   A copy of this program with the name X (file type &FF8) runs the script  scripts.X  of its own folder when you double-click it, or type its name.  Why not an Obey file: a program that is started from an
   Obey file cannot start a child program and come back (the swap of UnixLib 64 fails or the parent vanishes after the first child), and a compiler run is a driver that starts cc1, as, collect2 and ld.
   A program that is started by the Filer or typed at the command line can.

   The script language is the part of Obey that the samples use, one command to a line:
     | text           a comment            Obey NAME   runs the script scripts.NAME (to a depth of 4, with this program: no second Obey)
     anything else    a command of the operating system, run to its end (Echo, Dir, Set, SetType, WimpSlot, <T64$Dir>.xgcc ...).  The first command that returns a non-zero code stops the script.
   Before the first line it selects the file system of its folder, makes the folder the current directory (OS_FSControl 0: a Dir command run through system () is undone when it ends), sets T64$Dir to it and UnixEnv$swap to 1 (a program may start a program and wait for it).
   Build:  aarch64-riscos-gcc -munixlib -O2 -Wl,--defsym=__ul64_heap_max=0x400000 ulrun.c -o ulrun,ff8      (a small heap on purpose: the compilers that it starts need the memory)
   The SDK of RISC OS 64 (BSD licence like the rest of riscos64). */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <kernel.h>
#include <swis.h>

extern int __ul64_canon (const char *name, char *out, unsigned size);	/* OS_FSControl 37: the canonical name of a file or folder */
extern int __ul64_system_raw (const char *cmd);		/* the command line as it is, for the operating system (the SDK's process.c) */

static char folder[640];				/* the folder of the program, as a RISC OS name: FS::Disc.$.dir */

static int cli (const char *fmt, const char *arg)
{
  char cmd[1100];
  snprintf (cmd, sizeof cmd, fmt, arg);
  return __ul64_system_raw (cmd);
}

static int is_file (const char *path)				/* OS_File 17: is it a file? */
{
  _kernel_swi_regs r;
  r.r[0] = 17; r.r[1] = (int) (unsigned long) path;
  return _kernel_swi (OS_File, &r, &r) == 0 && r.r[0] == 1;
}

/* A name that lacks the file system (:Disc.$.dir: a program that was started by a program that swapped itself in gets its name that way) is looked for on each file system in turn. */
static int find_on_fs (const char *rest, const char *name)
{
  static char fsname[64], cand[640], path[800];
  int n;
  for (n = 0; n < 256; n++)
    {
      _kernel_swi_regs r;
      fsname[0] = 0;
      r.r[0] = 33; r.r[1] = n; r.r[2] = (int) (unsigned long) fsname; r.r[3] = sizeof fsname;	/* OS_FSControl 33: the name of file system number n */
      if (_kernel_swi (OS_FSControl, &r, &r) || !fsname[0]) continue;
      snprintf (cand, sizeof cand, "%s::%s", fsname, rest);
      snprintf (path, sizeof path, "%s.scripts.%s", cand, name);
      if (is_file (path)) { snprintf (folder, sizeof folder, "%s", cand); return 1; }
    }
  return 0;
}

static int run_script (const char *name, int depth)
{
  char path[1100], line[1100];
  FILE *f;
  int rc = 0;
  if (depth > 4) { printf ("ulrun: scripts call each other too deeply at %s\n", name); return 1; }
  snprintf (path, sizeof path, "%s.scripts.%s", folder, name);
  f = fopen (path, "r");
  if (!f) { printf ("ulrun: cannot read %s\n", path); return 1; }
  while (rc == 0 && fgets (line, sizeof line, f))
    {
      char *p = line, *e = line + strlen (line);
      while (e > p && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ')) *--e = 0;
      while (*p == ' ') p++;
      if (*p == 0 || *p == '|') continue;
      if (!strncasecmp (p, "obey ", 5)) { rc = run_script (p + 5, depth + 1); continue; }
      fflush (stdout);
      rc = __ul64_system_raw (p);
      if (rc != 0) printf ("ulrun: failed: %s (code %d)\n", p, rc);
    }
  fclose (f);
  return rc;
}

int main (int argc, char **argv)
{
  char name[128];
  const char *a0 = argc > 0 && argv[0] ? argv[0] : "", *sep, *colons;
  int rc;
  sep = strrchr (a0, '.');
  if (strrchr (a0, '/') > sep) sep = strrchr (a0, '/');
  if (!sep || sep == a0) { printf ("ulrun: cannot find my folder from '%s'\n", a0); return 1; }
  snprintf (folder, sizeof folder, "%.*s", (int) (sep - a0), a0);
  snprintf (name, sizeof name, "%s", sep + 1);
  if (folder[0] == '/')						/* a Unix name, /FS::Disc.$/dir/dir (UnixLib hands the program its name that way): FS::Disc.$.dir.dir */
    {
      char *q;
      memmove (folder, folder + 1, strlen (folder));
      for (q = folder; *q; q++) if (*q == '/') *q = '.';
    }
  {								/* the canonical name, FS::Disc.$.dir: a name that lacks the file system (:Disc.$.dir, $.dir or dir) is completed on the current file system, and Dir needs it */
    static char canon[512], path[800], rest[640];
    int on_disc_form = folder[0] == ':';
    snprintf (rest, sizeof rest, "%s", folder + (on_disc_form ? 1 : 0));
    if (__ul64_canon (folder, canon, sizeof canon) == 0 && canon[0]) snprintf (folder, sizeof folder, "%s", canon);
    snprintf (path, sizeof path, "%s.scripts.%s", folder, name);
    if (on_disc_form && !is_file (path) && !find_on_fs (rest, name)) { printf ("ulrun: cannot find %s on any file system\n", rest); return 1; }
  }
  colons = strstr (folder, "::");
  if (colons && colons - folder < 60)				/* FS::Disc.$.dir: select the file system first, as Dir does not */
    {
      char fs[64];
      snprintf (fs, sizeof fs, "%.*s", (int) (colons - folder), folder);
      cli ("%s", fs);
    }
  {								/* the current directory: not by a Dir command through system (): that puts the directory of this program back when the command has ended */
    _kernel_swi_regs r;
    r.r[0] = 0; r.r[1] = (int) (unsigned long) folder;		/* OS_FSControl 0: set the current directory */
    if (_kernel_swi (OS_FSControl, &r, &r)) { printf ("ulrun: cannot go to %s\n", folder); return 1; }
  }
  cli ("%%Set T64$Dir %s", folder);
  cli ("%s", "%Set UnixEnv$swap 1");
  printf ("ulrun: folder %s, script %s\n", folder, name);
  fflush (stdout);
  rc = run_script (name, 0);
  if (rc != 0) printf ("ulrun: stopped.\n");
  return rc;
}
