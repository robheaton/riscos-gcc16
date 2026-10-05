/* sultrace.h -- append lines to the SulLog file that the TRACED SharedUnixLibrary (SharedULib-116fix2t) writes to, with raw SWIs only (OS_Find 0xC0 = open for update, OS_Args 2 and 1 = go to the end,
   OS_GBPB 2 = write, OS_Find 0 = close), so that the program's own marks and the module's lines interleave in ONE file, in time order, and the last line survives a freeze that needs a reset.
   The line format is the module's:   <tag> v=<8 hex> sp=<8 hex> ctx=<8 hex> t=<8 hex> p=<8 hex> g=<8 hex> c=<8 hex> h=<8 hex>
       v = a value of the program's choice, sp = the program's stack pointer, ctx = VFPSupport_ActiveContext (ffffffff when the SWI fails), t = OS_ReadMonotonicTime (centiseconds),
       p = the word at 0x8038 (the pointer that PIC_LOAD uses: SOManager's GOTT array of this client), g = the word it points at (ffffffff when OS_ValidateAddress says no), c = the word at 0x8040
       (SOManager's client record of this application space: 0 after a SOM_DeregisterClient), h = the sum of the 64 words at 0x8000.
   Nothing here uses UnixLib but _swix: a vfork child may call it (it runs on the parent's stack, in the parent's image).  The file must exist (the module's trace opens it for update).  */
#ifndef SULTRACE_H
#define SULTRACE_H
#include <stdint.h>
#include <kernel.h>
#include <swis.h>

#define SUL_LOG "LanMan98::MyShare.$.SulLog"

#ifndef SL_PUT_HOST                                     /* (a host test build supplies its own sl_put) */
static void sl_put(const char *s, int n)               /* append N bytes of S: open, go to the end, write, close */
{
  unsigned h = 0, ext = 0;
  if (_swix(OS_Find, _INR(0, 1) | _OUT(0), 0xC0, SUL_LOG, &h) != NULL || h == 0) return;
  _swix(OS_Args, _INR(0, 1) | _OUT(2), 2, h, &ext);
  _swix(OS_Args, _INR(0, 2), 1, h, ext);
  _swix(OS_GBPB, _INR(0, 3), 2, h, s, n);
  _swix(OS_Find, _INR(0, 1), 0, h);
}
#endif

#ifndef SL_RD                                           /* (a host test build supplies its own SL_RD: a read of one word at an absolute address) */
#define SL_RD(a) (*(const volatile unsigned *) (uintptr_t) (a))
#endif

static char *sl_str(char *p, const char *s) { while (*s) *p++ = *s++; return p; }
static char *sl_hex8(char *p, unsigned v) { for (int i = 28; i >= 0; i -= 4) { unsigned d = (v >> i) & 15; *p++ = (char) (d < 10 ? '0' + d : 'A' + d - 10); } return p; }

/* one line: TAG v=V sp=<the caller's frame> ctx=<active VFP context> t=<time> */
static __attribute__((noinline)) void sl_mark(const char *tag, unsigned v)
{
  char b[260], *p = b;
  unsigned ctx = 0xFFFFFFFFu, t = 0;
  _swix(0x58EC6, _OUT(0), &ctx);                        /* VFPSupport_ActiveContext */
  _swix(OS_ReadMonotonicTime, _OUT(0), &t);
  unsigned pw = SL_RD(0x8038), cw = SL_RD(0x8040), hw = 0, gw = 0xFFFFFFFFu;
  for (int i = 0; i < 64; i++) hw += SL_RD(0x8000 + 4 * i);
  {                                                      /* g = [p] only when OS_ValidateAddress (X) answers: no error and C clear */
    _kernel_swi_regs rin, rout; int carry = 1;
    rin.r[0] = (int) pw; rin.r[1] = (int) (pw + 4);
    if (_kernel_swi_c(0x2003A, &rin, &rout, &carry) == NULL && !carry) gw = SL_RD(pw);
  }
  for (const char *q = tag; *q && p < b + 150; ) *p++ = *q++;               /* the tag is cut at 150 characters: the rest of the line is 92 at most, the buffer 260 */
  p = sl_str(p, " v="); p = sl_hex8(p, v);
  p = sl_str(p, " sp="); p = sl_hex8(p, (unsigned) (uintptr_t) __builtin_frame_address(0));
  p = sl_str(p, " ctx="); p = sl_hex8(p, ctx);
  p = sl_str(p, " t="); p = sl_hex8(p, t);
  p = sl_str(p, " p="); p = sl_hex8(p, pw);
  p = sl_str(p, " g="); p = sl_hex8(p, gw);
  p = sl_str(p, " c="); p = sl_hex8(p, cw);
  p = sl_str(p, " h="); p = sl_hex8(p, hw);
  *p++ = '\n';
  sl_put(b, (int) (p - b));
}
#endif
