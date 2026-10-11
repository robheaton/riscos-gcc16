/* dltest.c - DeskLib32 (built by recipe/gcc-16.2.0-riscos/scripts/install-desklib.py with GCC 16 and asasm) in a Task window: the assembler veneers and the C functions that need no Wimp task.
   Build:  arm-riscos-gnueabihf-gcc -O2 -o dltest,e1f dltest.c -lDesk       Run in a Task window (Ctrl-F12):  dltest   -> "dltest: N checks, 0 FAILED" */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "DeskLib:Core.h"
#include "DeskLib:Environment.h"
#include "DeskLib:File.h"
#include "DeskLib:Filing.h"
#include "DeskLib:Hourglass.h"
#include "DeskLib:Kbd.h"
#include "DeskLib:Module.h"
#include "DeskLib:Screen.h"
#include "DeskLib:Str.h"
#include "DeskLib:SWI.h"
#include "DeskLib:Time.h"

static int checks, failed;
#define CHECK(c, what) do { checks++; if (c) printf ("  ok    %s\n", what); else { failed++; printf ("  FAIL  %s\n", what); } } while (0)

#ifdef DLTEST_MODULE
int dltest_run (void)                                                      /* (the same checks, in the module DLMod: *DLMod_Test runs them in SVC mode) */
#else
int main (void)
#endif
{
  char buf[256], name[256];
  const char *scrap = getenv ("Wimp$ScrapDir");
  printf ("dltest: DeskLib32 built with GCC 16\n");

  /* the strings that end in a carriage return (assembler) */
  { char a[32] = "abc\r", b[32] = "xyz\r";
    CHECK (strlencr (a) == 3, "strlencr");
    CHECK (strcmpcr (a, "abc\r") == 0 && strcmpcr (a, b) < 0 && strcmpcr (b, a) > 0, "strcmpcr");
    strcpycr (buf, a); CHECK (!memcmp (buf, "abc\0", 4), "strcpycr (the copy is zero terminated)");
    strcatcr (buf, b); CHECK (!memcmp (buf, "abcxyz\0", 7), "strcatcr (zero terminated)");
    CHECK (stricmp ("Hello", "hELLO") == 0 && stricmp ("a", "b") < 0 && strnicmp ("HELPx", "helpy", 4) == 0, "stricmp, strnicmp");
    CHECK (!strcmp (Str_LeafName ("NVMe::Disc.$.a.b.leaf"), "leaf"), "Str_LeafName"); }

  /* the system variables, the string expansion (OS_GSTrans) and OS_CLI */
  CHECK (scrap != NULL && Environment_SysVarRead ("Wimp$ScrapDir", buf, sizeof buf) && !strcmp (buf, scrap), "Environment_SysVarRead");
  CHECK (Environment_SysVarSet ("DLTest$Var", "hello world", sysvar_STRING) == NULL && Environment_SysVarRead ("DLTest$Var", buf, sizeof buf) && !strcmp (buf, "hello world"), "Environment_SysVarSet and read");
  { char *end = NULL; os_error *e = Environment_ExpandString ("<DLTest$Var> there", buf, sizeof buf, &end);
    CHECK (e == NULL && !strcmp (buf, "hello world there") && end != NULL, "Environment_ExpandString (OS_GSTrans veneer)"); }
  CHECK (Environment__OS_CLI ("Set DLTest$Var2 from-oscli") == NULL && Environment_SysVarRead ("DLTest$Var2", buf, sizeof buf) && !strcmp (buf, "from-oscli"), "Environment__OS_CLI");
  CHECK (Environment_SysVarDelete ("DLTest$Var") == NULL && !Environment_SysVarRead ("DLTest$Var", buf, sizeof buf), "Environment_SysVarDelete");
  Environment_SysVarDelete ("DLTest$Var2");
  { int v = Environment_OSVersion (); printf ("  info  OS version %d\n", v); CHECK (v >= 350, "Environment_OSVersion"); }

  /* files: made, measured, looked at and deleted with DeskLib's own calls */
  if (scrap) { snprintf (name, sizeof name, "%s.DLTest", scrap); name[sizeof name - 1] = 0; }
  else strcpy (name, "DLTest");
  File_Delete (name);
  { file_handle h = File_Open (name, file_WRITE);
    CHECK (h != 0, "File_Open for writing");
    if (h) { CHECK (File_WriteBytes (h, "hello, DeskLib", 14) == NULL, "File_WriteBytes"); CHECK (File_Close (h) == NULL, "File_Close"); } }
  CHECK (File_Exists (name) && File_Size (name) == 14, "File_Exists and File_Size (14)");
  { file_handle h = File_Open (name, file_READ); char r[32]; int n;
    memset (r, 0, sizeof r);
    n = h ? File_ReadBytes (h, r, 5) : -1;                                /* (DeskLib returns the number of bytes NOT read) */
    CHECK (n == 0 && !memcmp (r, "hello", 5) && h && File_ReturnPos (h) == 5 && !File_EOF (h), "File_ReadBytes and File_ReturnPos");
    if (h) { File_Seek (h, 14); CHECK (File_EOF (h), "File_Seek and File_EOF"); File_Close (h); } }
  { filing_objtype ot = filing_NOTFOUND; int load = 0, exec = 0, len = 0, attr = 0, ft = 0; os_error *e = Filing_ReadCatalogue (name, &ot, &load, &exec, &len, &attr, &ft);
    CHECK (e == NULL && ot == filing_FILE && len == 14 && (ft == 0xFFD || ft == 0xFFF || ft >= 0), "Filing_ReadCatalogue (a file of 14 bytes)"); }
  { char full[256]; Filing_GetPathname (name, full); CHECK (full[0] != 0, "Filing_GetPathname"); }
  CHECK (File_Delete (name) == NULL && !File_Exists (name), "File_Delete");

  /* time */
  { unsigned t1 = Time_Monotonic (), t2; unsigned char clk[5]; int spin = 0;
    do { t2 = Time_Monotonic (); spin++; } while (t2 == t1 && spin < 100000000);
    CHECK (t2 > t1, "Time_Monotonic runs");
    memset (clk, 0, sizeof clk);
    CHECK (Time_ReadClock (clk) == NULL && (clk[0] | clk[1] | clk[2] | clk[3] | clk[4]) != 0, "Time_ReadClock");
    { char text[64] = "";
      CHECK (Time_ConvertDateAndTime (clk, text, sizeof text, "%YR-%MN-%DY") == NULL && strlen (text) >= 8, "Time_ConvertDateAndTime");
      printf ("  info  the date is %s\n", text); } }

  /* the screen and the keyboard */
  CHECK (Screen_CacheModeInfo () && screen_size.x > 0 && screen_size.y > 0 && screen_bpp > 0 && screen_delta.x == (1 << screen_eig.x) && screen_size.x == (screen_size.x >> screen_eig.x) * screen_delta.x, "Screen_CacheModeInfo (size in OS units, eigen values, bits per pixel)");
  printf ("  info  screen %d x %d pixels (screen_size is in OS units: %d x %d, eig %d,%d), %d bpp\n", screen_size.x >> screen_eig.x, screen_size.y >> screen_eig.y, screen_size.x, screen_size.y, screen_eig.x, screen_eig.y, screen_bpp);
  { int i, down = 0; for (i = 0; i < 4; i++) down += Kbd_KeyDown (0x7F + i) ? 1 : 0; CHECK (down >= 0, "Kbd_KeyDown can be called"); }
  { kbd_modifiers m = Kbd_GetModifiers (FALSE); (void) m; CHECK (1, "Kbd_GetModifiers can be called"); }

  /* a block from the RMA, the generic SWI veneer, the hourglass */
  { void *blk = NULL; CHECK (Module_Claim (100, &blk) == NULL && blk != NULL, "Module_Claim"); if (blk) CHECK (Module_Free (blk) == NULL, "Module_Free"); }
  { char num[16] = ""; char *end = NULL;
    { int r0 = -1; CHECK (SWI (3, 2, 0xD8, 12345, num, (int) sizeof num, &r0, &end) == NULL && !strcmp (num, "12345") && end == num + 5 && r0 == (int) num, "SWI (OS_ConvertCardinal4): the generic SWI veneer with three inputs and two outputs (R0 = the buffer, R1 = its end)"); } }
  { char swiname[64] = ""; int n = -1;
    CHECK (SWI_SWINumberToString (0x400C0, swiname, sizeof swiname) == NULL && !strcmp (swiname, "Wimp_Initialise"), "SWI_SWINumberToString (Wimp_Initialise)");
    CHECK (SWI_SWINumberFromString ("OS_WriteC", &n) == NULL && n == 0, "SWI_SWINumberFromString (OS_WriteC)"); }
  Hourglass_On (); Hourglass_Percentage (50); Hourglass_Off (); CHECK (1, "Hourglass_On, Hourglass_Percentage, Hourglass_Off can be called");

  printf ("dltest: %d checks, %d FAILED\n", checks, failed);
  return failed != 0;
}
