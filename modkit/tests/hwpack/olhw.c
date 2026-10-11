/* olhw.c - OSLib's own library (libOSLib32.a made from the OSLib sources by install-oslib.py) in a module, on the machine: the commands run in SVC mode, where a SWI overwrites LR, so every veneer
   must keep it (the non-X forms with up to four arguments need the -svc-safe patch of DefMod).  *OlHw_Test checks the answers of the real SWIs; *OlHw_Raise fails a non-X call (the error ends the command). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kernel.h"
#include "oslib/os.h"
#include "oslib/osfile.h"
#include "oslib/osfind.h"
#include "oslib/osargs.h"
#include "oslib/fileswitch.h"
#include "header.h"

static int checks, failed;
#define CHECK(c, what) do { checks++; if (c) printf ("  ok    %s\n", what); else { failed++; printf ("  FAIL  %s\n", what); } } while (0)

_kernel_oserror *olhw_table (const char *arg_string, int argc, int number, void *pw)
{
  char buf[200], name[260], scrap[200];
  const char *sd = getenv ("Wimp$ScrapDir");
  os_error *e;
  (void) arg_string; (void) argc; (void) pw;
  if (number == CMD_OlHw_Raise)
    {
      int used;
      printf ("OlHw_Raise: calling os_read_var_val on a variable that does not exist (a non-X call that fails)...\n");
      os_read_var_val ("OlHw$Missing", buf, sizeof buf, 0, os_VARTYPE_STRING, &used, 0);
      printf ("AFTER: if you can read this the error did not end the command line\n");
      return 0;
    }
  checks = failed = 0;
  printf ("OlHw: OSLib's own libOSLib32.a in a module (SVC mode)\n");

  /* the non-X forms with few arguments: they must keep LR (-svc-safe) */
  { unsigned t1 = os_read_monotonic_time (), t2 = os_read_monotonic_time ();
    CHECK (t2 >= t1 && t1 != 0, "os_read_monotonic_time (a non-X veneer with no input: LR kept through the SWI)"); }
  { char *r = os_convert_cardinal4 (777, buf, sizeof buf);
    CHECK (r == buf + 3 && !strcmp (buf, "777"), "os_convert_cardinal4 (a non-X veneer with three inputs; OSLib: Returns R1 = the end of the text)"); }
  os_write0 ("  info  os_write0 and os_new_line work: this line is written by os_write0");
  os_new_line ();

  /* the X forms: outputs and errors */
  { char *end = 0; e = xos_convert_cardinal4 (12345, buf, sizeof buf, &end);
    CHECK (e == NULL && !strcmp (buf, "12345") && end == buf + 5, "xos_convert_cardinal4 (the end pointer in R1)"); }
  { os_t t = 0; e = xos_read_monotonic_time (&t); CHECK (e == NULL && t != 0, "xos_read_monotonic_time"); }
  { int n = -1; e = xos_swi_number_from_string ("OS_WriteC", &n); CHECK (e == NULL && n == 0, "xos_swi_number_from_string (OS_WriteC)");
    e = xos_swi_number_from_string ("No_SuchSwiAtAll", &n); CHECK (e != NULL, "xos_swi_number_from_string of a name that is not a SWI: the error block"); }

  /* system variables: seven arguments (some on the stack), outputs in several registers */
  { int ctx = -1; os_var_type vt = -1;
    e = xos_set_var_val ("OlHw$Var", (byte const *) "hello", 5, 0, os_VARTYPE_STRING, &ctx, &vt);
    CHECK (e == NULL, "xos_set_var_val (seven arguments, LR kept on the stack)"); }
  { int used = -1, ctx = -1; os_var_type vt = -1; memset (buf, 0, sizeof buf);
    e = xos_read_var_val ("OlHw$Var", buf, sizeof buf, 0, os_VARTYPE_STRING, &used, &ctx, &vt);
    CHECK (e == NULL && used == 5 && !memcmp (buf, "hello", 5) && vt == os_VARTYPE_STRING, "xos_read_var_val (eight arguments, three outputs)"); }
  { int used = -1; e = xos_gs_trans ("<OlHw$Var> there", buf, sizeof buf, &used, 0);
    CHECK (e == NULL && used == 11 && !memcmp (buf, "hello there", 11), "xos_gs_trans (<variable> expanded)"); }
  CHECK (xos_cli ("Set OlHw$Cli yes") == NULL, "xos_cli");
  { int used = -1; e = xos_read_var_val ("OlHw$Cli", buf, sizeof buf, 0, os_VARTYPE_STRING, &used, 0, 0); CHECK (e == NULL && used == 3 && !memcmp (buf, "yes", 3), "the variable that xos_cli set"); }
  { int used; e = xos_read_var_val ("OlHw$Missing", buf, sizeof buf, 0, os_VARTYPE_STRING, &used, 0, 0);
    CHECK (e != NULL && e->errnum == 0x124 && strstr (e->errmess, "OlHw$Missing"), "xos_read_var_val of a variable that is not there: the error block (&124)"); }
  xos_cli ("Unset OlHw$Var"); xos_cli ("Unset OlHw$Cli");

  /* the display */
  { int xeig = -1, ywl = -1; e = xos_read_mode_variable (os_CURRENT_MODE, os_MODEVAR_XEIG_FACTOR, &xeig, 0);
    CHECK (e == NULL && xeig >= 0 && xeig <= 4, "xos_read_mode_variable (XEigFactor)");
    e = xos_read_mode_variable (os_CURRENT_MODE, os_MODEVAR_YWIND_LIMIT, &ywl, 0); CHECK (e == NULL && ywl > 100, "xos_read_mode_variable (YWindLimit)");
    printf ("  info  XEigFactor %d, YWindLimit %d (%d pixels high)\n", xeig, ywl, ywl + 1); }

  /* files: OS_File with several arguments, OS_Find, OS_Args, OS_File 6 (delete) */
  if (sd) snprintf (scrap, sizeof scrap, "%s", sd); else strcpy (scrap, "@");
  snprintf (name, sizeof name, "%s.OlHw", scrap);
  { static const byte data[14] = "hello, OSLib!";
    xosfile_delete (name, 0, 0, 0, 0, 0);
    e = xosfile_save_stamped (name, 0xFFD, data, data + sizeof data);
    CHECK (e == NULL, "xosfile_save_stamped (four arguments)"); }
  { fileswitch_object_type ot = -1; bits load = 0, exec = 0, ft = 0; int size = -1; fileswitch_attr attr = 0;
    e = xosfile_read_stamped_no_path (name, &ot, &load, &exec, &size, &attr, &ft);
    CHECK (e == NULL && ot == fileswitch_IS_FILE && size == 14 && ft == 0xFFD, "xosfile_read_stamped_no_path (type 1, 14 bytes, file type &FFD)"); }
  { os_fw h = 0; int ext = -1;
    e = xosfind_openinw (osfind_NO_PATH, name, 0, &h);
    CHECK (e == NULL && h != 0, "xosfind_openinw");
    if (e == NULL && h) { e = xosargs_read_ext (h, &ext); CHECK (e == NULL && ext == 14, "xosargs_read_ext (14)"); e = xosfind_close (h); CHECK (e == NULL, "xosfind_close"); } }
  { fileswitch_object_type ot = -1; e = xosfile_delete (name, &ot, 0, 0, 0, 0); CHECK (e == NULL && ot == fileswitch_IS_FILE, "xosfile_delete (the object that was deleted is a file)"); }
  { fileswitch_object_type ot = -1; bits l, x, f; int s; fileswitch_attr a; e = xosfile_read_stamped_no_path (name, &ot, &l, &x, &s, &a, &f);
    CHECK (e == NULL && ot == fileswitch_NOT_FOUND, "the file is gone (object type 0)"); }

  printf ("OlHw_Test: %d checks, %d FAILED\n", checks, failed);
  return 0;
}
