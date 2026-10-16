/* swi64.c - _swi and _swix in a module: inputs, outputs, flags, _RETURN, an error, a _BLOCK.   aarch64-riscos-gcc -mmodule -O2 swi64.c swi64_hdr.o -o Swi64 */
#include <stdio.h>
#include <string.h>
#include "kernel.h"
#include "swis.h"
#include "swi64h.h"

static unsigned run_checks (void)
{
  unsigned ok = 0, used = 0, mono1, mono2, flags = 0, r2 = 0, r3 = 0;
  char buf[64];
  _kernel_oserror *e;

  /* OS_ReadVarVal with an unknown variable: an error */
  e = _swix (OS_ReadVarVal, _INR (0, 4), "Swi64$NoSuchThing", buf, sizeof buf, 0, 3);
  if (e) ok |= 1u << 0;
  /* OS_SetVarVal / OS_ReadVarVal: string in, string out, the length in r2 */
  e = _swix (OS_SetVarVal, _INR (0, 4), "Swi64$Var", "some text", 9, 0, 0);
  if (!e) ok |= 1u << 1;
  e = _swix (OS_ReadVarVal, _INR (0, 4) | _OUT (2), "Swi64$Var", buf, sizeof buf, 0, 3, &used);
  if (!e && used == 9 && !memcmp (buf, "some text", 9)) ok |= 1u << 2;
  _swix (OS_SetVarVal, _INR (0, 4), "Swi64$Var", 0, (unsigned) -1, 0, 0);               /* delete it */
  /* _swi with _RETURN: OS_ReadMonotonicTime */
  mono1 = (unsigned) _swi (OS_ReadMonotonicTime, _RETURN (0));
  mono2 = (unsigned) _swi (OS_ReadMonotonicTime, _RETURN (0));
  if (mono2 >= mono1) ok |= 1u << 3;
  /* the flags: OS_ReadUnsigned (r0 = base | flags, r1 -> text): r2 = the value, r1 advanced */
  e = _swix (OS_ReadUnsigned, _INR (0, 2) | _OUT (2) | _OUT (_FLAGS), 10, "12345", 0, &r2, &flags);
  if (!e && r2 == 12345) ok |= 1u << 4;
  /* several outputs: OS_ReadUnsigned returns r1 (the end of the text) and r2 */
  { const char *text = "77 rest"; unsigned end = 0;
    e = _swix (OS_ReadUnsigned, _INR (0, 2) | _OUTR (1, 2), 10, text, 0, &end, &r3);
    if (!e && r3 == 77 && end == (unsigned) (unsigned long) text + 2) ok |= 1u << 5; }
  /* _vswix through a wrapper */
  { unsigned v = 0; e = _swix (OS_ReadUnsigned, _INR (0, 2) | _OUT (2), 16, "ff", 0, &v); if (!e && v == 255) ok |= 1u << 6; }
  /* _BLOCK: the rest of the arguments are the words of a block that r1 points at.  OS_Word 14 would need a buffer; OS_CLI needs none: use Wimp-free OS_ConvertCardinal4 instead: r0 = value, r1 -> buffer, r2 = size */
  e = _swix (OS_ConvertCardinal4, _INR (0, 2), 4000000000u, buf, sizeof buf);
  if (!e && !strcmp (buf, "4000000000")) ok |= 1u << 7;
  /* an error from a SWI that does not exist: _swix returns it */
  e = _swix (0x1FF0FF, 0);
  if (e && (e->errnum & 0xFFFF) == 0x1E6) ok |= 1u << 8;
  /* _kernel_last_oserror remembers it */
  if (_kernel_last_oserror () != 0) ok |= 1u << 9;
  /* _BLOCK(2): OS_Hardware-free test with the test SWI OS_Byte 129? use OS_GetEnv: no input.  Here: OS_ReadSysInfo-free: pass 3 words as a block to OS_Word 0? skip: use OS_File 17 with a block-less call */
  e = _swix (OS_File, _INR (0, 1) | _OUT (4), 17, "$", &r3);
  if (!e) ok |= 1u << 10;
  /* _BLOCK (0): the words that follow are the block that r0 points at: OS_ReadVduVariables reads the list of variable numbers (here 11 = ModeFlags... and the end mark -1) and writes the values to r1 */
  { unsigned out[2] = { 0xDEADBEEFu, 0xDEADBEEFu };
    e = _swix (OS_ReadVduVariables, _IN (1) | _BLOCK (0), out, 128u, 0xFFFFFFFFu);
    if (!e && out[0] != 0xDEADBEEFu) ok |= 1u << 11; }
  return ok;
}

_kernel_oserror *sw_swi (int swi_offset, _kernel_swi_regs *r, void *pw)
{
  if (swi_offset != 0) return error_BAD_SWI;
  r->r[0] = (int) run_checks ();
  return 0;
}
