/* hosthooks.c - the kernel as the host build of the library tests sees it: __modlib_xswi / __modlib_xswif of modswi.S, and the model of the test SWIs (the same model is in armrun.py for the interpreter):
     0x5AB00  r0 = r0 + r1, r1 = r2 ^ 0x1234, r2 = r3 * 3, r3 = r4 - r5, r4 = r6 | r7, r5 = r8, r6 = r9, r7 = the old r0, r8 = 0x80000000, r9 = 0xFFFFFFFF; flags: N and Z of the new r0, C when the old r0 was odd;
             r1 = 0xBAD on entry: an error (number 0xB00B, "Test error"), V set
     0x5AB01  sets the real time clock (r0: low 32 bits, r1: bits 32 - 39, centiseconds since 1900) as OS_Word 14, 3 reads it
     0x5AB02  sets the value that OS_ReadMonotonicTime gives (r0) */
#include <string.h>

typedef struct { int errnum; char errmess[252]; } oserr;
static oserr test_err = { 0xB00B, "Test error" }, unknown_err = { 0x1E6, "SWI not known to the host model" };
static unsigned long long g_clock;
static unsigned g_mono;

int mk_host_read_clock (unsigned char *b)
{
  for (int i = 0; i < 5; i++) b[i] = (unsigned char) (g_clock >> (8 * i));
  return 0;
}
oserr *__modlib_xswif (unsigned swi, unsigned *r, unsigned *flags)
{
  unsigned n = swi & ~0x20000u, fl = 0;
  oserr *err = 0;
  switch (n)
    {
    case 0x5AB00:
      {
	unsigned in0 = r[0], o[10];
	if (r[1] == 0xBAD) { err = &test_err; break; }
	o[0] = r[0] + r[1]; o[1] = r[2] ^ 0x1234; o[2] = r[3] * 3; o[3] = r[4] - r[5]; o[4] = r[6] | r[7]; o[5] = r[8]; o[6] = r[9]; o[7] = in0; o[8] = 0x80000000u; o[9] = 0xFFFFFFFFu;
	memcpy (r, o, sizeof o);
	fl = (o[0] & 0x80000000u) | (o[0] == 0 ? 0x40000000u : 0) | ((in0 & 1) ? 0x20000000u : 0);
	break;
      }
    case 0x5AB01: g_clock = r[0] | ((unsigned long long) (r[1] & 0xFF) << 32); break;
    case 0x5AB02: g_mono = r[0]; break;
    case 0x42: r[0] = g_mono; break;
    default: err = &unknown_err;
    }
  if (err) { if (flags) *flags = 0x10000000u; return err; }
  if (flags) *flags = fl;
  return 0;
}
oserr *__modlib_xswi (unsigned swi, unsigned *r) { return __modlib_xswif (swi, r, 0); }
