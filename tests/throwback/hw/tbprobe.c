/* tbprobe.c -- what is there for throwback on this machine?  READ-ONLY: it changes nothing.
   Shows: whether the desktop is running, whether the DDEUtils module is loaded and its version, where a DDEUtils module file may be on the disk, which tasks are running (an
   editor that does throwback - StrongED, Zap, Edit, SrcEdit ... - must be among them, and must have registered with DDEUtils), and the editor that opens text files.
   usage: tbprobe [--need-dde]      with --need-dde the exit status is 0 only when DDEUtils is loaded and the desktop is running.  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <swis.h>
#include <kernel.h>


static const char *var (const char *name)
{
  static char buf[512];
  int len = 0;
  /* R2 = 0 on exit when the variable is not set (no error); type 3 = the value as a string, with its macros expanded */
  _kernel_oserror *e = _swix (OS_ReadVarVal, _INR (0, 4) | _OUT (2), name, buf, (int) sizeof buf - 1, 0, 3, &len);
  if (e || len <= 0 || len >= (int) sizeof buf) return NULL;
  buf[len] = 0;
  return buf;
}

static void show_var (const char *name)
{
  const char *v = var (name);
  printf ("  %-28s %s\n", name, v ? v : "(not set)");
}

static int file_type (const char *path)      /* 0 nothing, 1 file, 2 directory, 3 image file; -1 error */
{
  int type = 0;
  _kernel_oserror *e = _swix (OS_File, _INR (0, 1) | _OUT (0), 17, path, &type);
  return e ? -1 : type;
}

int main (int argc, char **argv)
{
  int need = argc > 1 && !strcmp (argv[1], "--need-dde");
  printf ("tbprobe 1.2: argc=%d", argc);
  for (int i = 0; i < argc; i++) printf (" [%s]", argv[i]);
  printf ("\n");

  int tasks = -1;
  _kernel_oserror *e = _swix (Wimp_ReadSysInfo, _IN (0) | _OUT (0), 0, &tasks);
  if (e) printf ("  Wimp_ReadSysInfo 0: %s\n", e->errmess);
  else printf ("  the desktop: %s (%d active task%s)\n", tasks > 0 ? "RUNNING" : "NOT running (throwback needs it)", tasks, tasks == 1 ? "" : "s");

  int mod = 0, inst = 0, base = 0, priv = 0, dde = 0;
  e = _swix (OS_Module, _INR (0, 1) | _OUTR (1, 4), 18, "DDEUtils", &mod, &inst, &base, &priv);
  if (e) printf ("  DDEUtils: NOT loaded (%s)\n", e->errmess);
  else
    {
      dde = 1;
      const char *title = (const char *) (base + *(int *) (base + 0x10)), *help = (const char *) (base + *(int *) (base + 0x14));
      printf ("  DDEUtils: loaded, module number %d, \"%s\", help \"%s\"\n", mod, title, help);
      int n = -1;
      e = _swix (0x42583 /* DDEUtils_GetCLSize */, _OUT (0), &n);
      printf ("  DDEUtils_GetCLSize: %s (now %d)\n", e ? e->errmess : "ok (the module answers its SWIs)", n);
    }

  printf ("  module files that could be loaded:\n");
  static const char *cand[] = { "System:Modules.DDEUtils", "System:310.Modules.DDEUtils", "System:350.Modules.DDEUtils", "System:400.Modules.DDEUtils", "System:500.Modules.DDEUtils",
				"Resources:$.Resources.DDEUtils", "Resources:$.Resources.System.Modules.DDEUtils", "<Boot$Dir>.Resources.DDEUtils", "<StrongED$Dir>.Resources.DDEUtils",
				"<StrongED$Dir>.Modules.DDEUtils", "<Zap$Dir>.Modules.DDEUtils", "<Edit$Dir>.DDEUtils", "<Edit$Dir>.Modules.DDEUtils", "<SrcEdit$Dir>.Modules.DDEUtils",
				"<Obey$Dir>.DDEUtils", NULL };
  int found = 0;
  for (int i = 0; cand[i]; i++)
    {
      int t = file_type (cand[i]);
      if (t == 1 || t == 3) { printf ("    FOUND  %s\n", cand[i]); found++; }
    }
  if (!found) printf ("    (none of %d usual places)\n", (int) (sizeof cand / sizeof cand[0]) - 1);

  printf ("  the editors and the tasks they run in (variables):\n");
  show_var ("Alias$@RunType_FFF");
  show_var ("StrongED$Dir");
  show_var ("StrongED$Path");
  show_var ("Zap$Dir");
  show_var ("Edit$Dir");
  show_var ("SrcEdit$Dir");
  show_var ("DDE$Dir");
  show_var ("DDEUtils$Dir");
  show_var ("System$Path");

  printf ("  the variables that the compiler reads:\n");
  static const char *cvars[] = { "Sys$RCLimit", "Run$Path", "UnixEnv$gcc$sfix", "UnixEnv$cc1$sfix", "TMPDIR", "Wimp$ScrapDir", "GCC16$Dir", "GCC16bin$Path", "GCC_EXEC_PREFIX", "COMPILER_PATH",
				 "LIBRARY_PATH", "C_INCLUDE_PATH", "CPATH", "Prefix$Dir", "THROWBACK_DEBUG", "THROWBACK_HOST", NULL };
  for (int i = 0; cvars[i]; i++) show_var (cvars[i]);
  {
    const char *prefix = NULL;
    e = _swix (0x4258A /* DDEUtils_ReadPrefix */, _IN (0) | _OUT (0), 0, &prefix);
    if (e) printf ("  DDEUtils prefix of this task: %s\n", e->errmess);
    else if (!prefix) printf ("  DDEUtils prefix of this task: none\n");
    else { char b[300]; int n = 0; while (n < 299 && (unsigned char) prefix[n] >= ' ') { b[n] = prefix[n]; n++; } b[n] = 0; printf ("  DDEUtils prefix of this task: \"%s\"\n", b); }
  }

  printf ("  tasks running now (TaskManager_EnumerateTasks):\n");
  int index = 0, count = 0;
  while (index >= 0 && count < 200)
    {
      int buf[16 * 4];
      int next = -1;
      e = _swix (TaskManager_EnumerateTasks, _INR (0, 2) | _OUT (0), index, buf, (int) sizeof buf, &next);
      if (e) { printf ("    %s\n", e->errmess); break; }
      /* R1 on exit points after the last record written; the records are 4 words: handle, name pointer, memory, flags */
      int used = 0;
      _swix (TaskManager_EnumerateTasks, _INR (0, 2) | _OUTR (0, 1), index, buf, (int) sizeof buf, &next, &used);
      int words = (used - (int) buf) / 4;
      for (int w = 0; w + 3 < words; w += 4)
	{
	  {
	    const char *tn = (const char *) buf[w + 1];
	    char nb[64]; int k = 0;
	    while (k < 63 && (unsigned char) tn[k] >= ' ') { nb[k] = tn[k]; k++; }
	    nb[k] = 0;
	    printf ("    task %4d  %s\n", buf[w], nb);
	  }
	  count++;
	}
      index = next;
    }
  printf ("  (a throwback editor is one of these: StrongED, Zap, !Edit, SrcEdit; it also has to register with DDEUtils, which it does when DDEUtils is loaded)\n");

  printf ("tbprobe done\n");
  if (need && !(dde && tasks > 0)) return 1;
  return 0;
}
