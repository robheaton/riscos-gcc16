/* a64hooks.c - the kernel and the host as the AArch64 build of the library test (libtest.c, T_HOSTLIB) sees them.  The library is compiled with MODLIB_HOST, as it is for the host build, and calls the same
   hooks as tests/libtest/hosthooks.c has; this file is the AArch64 version.  A register of a SWI has 32 bits in the library (unsigned r[10]), a pointer has 64: on RISC OS 64 every address of a program is
   below 4 GB (the program is linked at &8000, the RAM is a few megabytes), so a pointer in a register is the address itself (mk_ptr_in), and the library's own casts (unsigned) p are the address too.
   Without SWIS no SWI is executed: the sections of the test that only compute (ctype, string, numbers, printf into a buffer, strtod, the floating point and the maths ...) do not call one, and a SWI that is
   asked for gets "not known".  With SWIS=1 the SWIs of the file system, the clock and the screen are passed on to the kernel (the instruction svc #0 with the number in x10: a64swi.S).
   Either way the two system variables of the test, LT_ONLY and LT_VERBOSE, are answered here from the command line (a64main.c), and the test SWIs that tests/libtest/hosthooks.c models are modelled here the
   same way (&5AB00 arithmetic with flags and an error, &5AB01 set the clock, &5AB02 set OS_ReadMonotonicTime, the screen that is captured, the keyboard that the test fills); the models of the file system
   (and its faults, &5AB07) are not here: with SWIS=1 the files are the machine's own. */
#include <stddef.h>
#include <string.h>

typedef struct { int errnum; char errmess[252]; } oserr;
static oserr unknown_err = { 0x1E6, "SWI not known to the AArch64 test" };
static oserr e_novar = { 0x124, "System variable not found" };

unsigned mk_ptr_in (const void *p) { return (unsigned) (unsigned long) p; }

const char *a64_only, *a64_verbose;
#ifdef SWIS
/* a64_swi: the registers of the block r[0..9] (32 bit slots) go to x0-x9, the SWI number to x10; on return x0-x9 are stored back (32 bit), the flags of the SWI are returned in *flags (N Z C V in bits 31 to 28
   as in the 32 bit PSR) and a SWI that is an error (V set) returns the error block that x0 points to */
extern oserr *a64_swi (unsigned swi, unsigned *r, unsigned *flags);
#endif

/* the clock, the screen and the keyboard of the tests (the same models as hosthooks.c) */
static oserr test_err = { 0xB00B, "Test error" };
static unsigned long long g_clock;
static unsigned g_mono;
static int mono_set;
static char screen[2048]; static size_t screen_len; static int screen_on;
void mk_host_screen_start (void) { screen_on = 1; screen_len = 0; }
size_t mk_host_screen_take (char *buf, size_t size)
{
  size_t n = screen_len < size ? screen_len : size;
  if (n) memcpy (buf, screen, n);
  screen_on = 0; screen_len = 0;
  return n;
}
static void screen_put (char c) { if (screen_len < sizeof screen) screen[screen_len++] = c; }
static struct { char *s; int len; } keys[32];
static char keybuf[32][256];
static int nkeys, kpos;
void mk_host_keys_push (const char *s, int len)
{
  if (nkeys >= 32) return;
  if (len >= 0) { if (len > 255) len = 255; memcpy (keybuf[nkeys], s, (size_t) len); keys[nkeys].s = keybuf[nkeys]; }
  else keys[nkeys].s = 0;
  keys[nkeys].len = len;
  nkeys++;
}
int mk_host_read_clock (unsigned char *b)
{
  for (int i = 0; i < 5; i++) b[i] = (unsigned char) (g_clock >> (8 * i));
  return 0;
}
void mk_host_fs_ctl (int reason, const char *name, unsigned value) { (void) reason; (void) name; (void) value; }          /* (faults of the file system cannot be made on the machine) */

/* returns 1 when the SWI was a model's: *err and *fl are then the result */
static int model (unsigned swi, unsigned *r, unsigned *flags, oserr **err)
{
  unsigned n = swi & ~0x20000u, fl = 0;
  *err = 0;
  switch (n)
    {
    case 0x5AB00:
      {
	unsigned in0 = r[0], o[10];
	if (r[1] == 0xBAD) { *err = &test_err; if (flags) *flags = 0x10000000u; return 1; }
	o[0] = r[0] + r[1]; o[1] = r[2] ^ 0x1234; o[2] = r[3] * 3; o[3] = r[4] - r[5]; o[4] = r[6] | r[7]; o[5] = r[8]; o[6] = r[9]; o[7] = in0; o[8] = 0x80000000u; o[9] = 0xFFFFFFFFu;
	memcpy (r, o, sizeof o);
	fl = (o[0] & 0x80000000u) | (o[0] == 0 ? 0x40000000u : 0) | ((in0 & 1) ? 0x20000000u : 0);
	break;
      }
    case 0x5AB01: g_clock = r[0] | ((unsigned long long) (r[1] & 0xFF) << 32); break;
    case 0x5AB02: g_mono = r[0]; mono_set = 1; break;
    case 0x42: if (!mono_set) return 0; r[0] = g_mono; break;                                    /* OS_ReadMonotonicTime, once the test has set it */
    case 0x00: if (!screen_on) return 0; screen_put ((char) r[0]); break;                        /* OS_WriteC, OS_NewLine (LF CR) while the screen is captured */
    case 0x03: if (!screen_on) return 0; screen_put ('\n'); screen_put ('\r'); break;
    case 0x0E:                                                                                   /* OS_ReadLine: the lines that the test has pushed (Escape when there is none) */
      if (!nkeys) return 0;
      if (kpos >= nkeys || keys[kpos].len < 0) { if (kpos < nkeys) kpos++; fl = 0x20000000u; break; }
      {
	int len = keys[kpos].len;
	if (len > (int) r[1]) len = (int) r[1];
	memcpy ((char *) (unsigned long) r[0], keys[kpos].s, (size_t) len);
	((char *) (unsigned long) r[0])[len] = '\r';
	r[1] = (unsigned) len;
	kpos++;
      }
      break;
    default: return 0;
    }
  if (flags) *flags = fl;
  return 1;
}

oserr *__modlib_xswif (unsigned swi, unsigned *r, unsigned *flags)
{
  { oserr *e; if (model (swi, r, flags, &e)) return e; }
#ifndef REALVARS
  if ((swi & ~0x20000u) == 0x23)                                    /* OS_ReadVarVal: LT_ONLY and LT_VERBOSE (REALVARS: the module build reads the real system variables) */
    {
      const char *name = (const char *) (unsigned long) r[0], *v = 0;
      int mine = 1;
      if (!strcmp (name, "LT_ONLY")) v = a64_only;
      else if (!strcmp (name, "LT_VERBOSE")) v = a64_verbose;
      else mine = 0;
      if (mine)
	{
	  if (v && strlen (v) < r[2])
	    {
	      char *buf = (char *) (unsigned long) r[1];
	      strcpy (buf, v);
	      r[2] = (unsigned) strlen (v);
	      if (flags) *flags = 0;
	      return 0;
	    }
	  if (flags) *flags = 0x10000000u;
	  return &e_novar;
	}
    }
#endif
#ifdef SWIS
  {
    unsigned f = 0;
    oserr *e = a64_swi (swi, r, &f);
    if (flags) *flags = f;
    return e;
  }
#else
  if (flags) *flags = 0x10000000u;
  return &unknown_err;
#endif
}
oserr *__modlib_xswi (unsigned swi, unsigned *r) { return __modlib_xswif (swi, r, 0); }

/* the hooks that exit.c has on the machine (the host build leaves exit.c out) */
#ifndef REALRAISE
void (*__modlib_stdio_end_hook) (int);
#endif

/* __modlib_raise (OS_GenerateError on the machine): the test has no error handler to go to; it only remembers the error */
const void *g_raised;
#ifdef REALRAISE				/* the module build: OS_GenerateError for real (exit () and abort () of a module stop with an error) */
void __modlib_raise (const void *e)
{
  unsigned r[10] = { (unsigned) (unsigned long) e };
  g_raised = e;
  a64_swi (0x2B, r, 0);
}
#else
void __modlib_raise (const void *e) { g_raised = e; }
#endif
