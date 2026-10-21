/* kernel.c - the _kernel_ veneers of kernel.h over swi_x (SDK of RISC OS 64).  The registers are 32 bit values (as in the 64-bit export of the RISC OS C library): they are
   zero extended into the 64-bit registers of the SWI and the results are truncated.  A "non-X" SWI number (bit 17 clear) is run as the X form like every call here, and
   its error is returned (there is no error handler to call). */
#include <errno.h>
#include <kernel.h>
#include <stdlib.h>
#include <string.h>

extern int swi_x (unsigned long swi, unsigned long *regs);	/* swi.S: the SWI with the X bit set; 1 if V was set, the error block address then is in regs[0] */
static _kernel_oserror last_error;

static _kernel_oserror *errorp (unsigned long e)
{
  const unsigned char *p = (const unsigned char *) e;
  last_error.errnum = (int) (p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned) p[3] << 24));
  strncpy (last_error.errmess, (const char *) p + 4, sizeof last_error.errmess - 1);
  last_error.errmess[sizeof last_error.errmess - 1] = 0;
  return &last_error;
}

_kernel_oserror *_kernel_swi (int no, _kernel_swi_regs *in, _kernel_swi_regs *out)
{
  unsigned long r[10]; int i;
  for (i = 0; i < 10; i++) r[i] = (unsigned int) in->r[i];
  if (swi_x ((unsigned long) ((unsigned) no & 0xFFFFF), r)) return errorp (r[0]);
  for (i = 0; i < 10; i++) out->r[i] = (int) r[i];
  return NULL;
}

_kernel_oserror *_kernel_swi_c (int no, _kernel_swi_regs *in, _kernel_swi_regs *out, int *carry)
{
  _kernel_oserror *e = _kernel_swi (no, in, out);
  if (carry) *carry = 0;			/* the carry flag is not given by swi_x */
  return e;
}

_kernel_oserror *_kernel_last_oserror (void) { return last_error.errnum || last_error.errmess[0] ? &last_error : NULL; }

static int run (int no, unsigned long *r) { if (swi_x ((unsigned long) no, r)) { errorp (r[0]); return -1; } return 0; }

int _kernel_osbyte (int op, int x, int y)
{
  unsigned long r[10] = { (unsigned int) op, (unsigned int) x, (unsigned int) y };
  if (run (6, r)) return _kernel_ERROR;
  return (int) ((r[1] & 0xFF) | ((r[2] & 0xFF) << 8));
}
int _kernel_osrdch (void) { unsigned long r[10] = { 0 }; if (run (4, r)) return _kernel_ERROR; return (int) (r[0] & 0xFF); }
int _kernel_oswrch (int ch) { unsigned long r[10] = { (unsigned int) ch & 0xFF }; return run (0, r) ? _kernel_ERROR : ch; }
int _kernel_osbget (int h) { unsigned long r[10] = { 0, (unsigned int) h }; if (run (0xA, r)) return _kernel_ERROR; return (int) (r[0] & 0xFF); }
int _kernel_osbput (int ch, int h) { unsigned long r[10] = { (unsigned int) ch & 0xFF, (unsigned int) h }; return run (0xB, r) ? _kernel_ERROR : ch; }
int _kernel_osgbpb (int op, int h, _kernel_osgbpb_block *b)
{
  unsigned long r[10] = { (unsigned int) op, (unsigned int) h, (unsigned long) b->dataptr, (unsigned int) b->nbytes, (unsigned int) b->fileptr, (unsigned int) b->buf_len, (unsigned long) b->wild_fld };
  if (run (0xC, r)) return _kernel_ERROR;
  b->dataptr = (void *) r[2]; b->nbytes = (int) r[3]; b->fileptr = (int) r[4];
  return 0;
}
int _kernel_osword (int op, int *data) { unsigned long r[10] = { (unsigned int) op, (unsigned long) data }; return run (7, r) ? _kernel_ERROR : 0; }
int _kernel_osfind (int op, char *name) { unsigned long r[10] = { (unsigned int) op, (unsigned long) name }; if (run (0xD, r)) return _kernel_ERROR; return (int) r[0]; }
int _kernel_osfile (int op, const char *name, _kernel_osfile_block *b)
{
  unsigned long r[10] = { (unsigned int) op, (unsigned long) name, (unsigned int) b->load, (unsigned int) b->exec, (unsigned int) b->start, (unsigned int) b->end };
  if (run (8, r)) return _kernel_ERROR;
  b->load = (int) r[2]; b->exec = (int) r[3]; b->start = (int) r[4]; b->end = (int) r[5];
  return (int) r[0];
}
int _kernel_osargs (int op, int h, int arg) { unsigned long r[10] = { (unsigned int) op, (unsigned int) h, (unsigned int) arg }; if (run (9, r)) return _kernel_ERROR; return (int) r[2]; }
int _kernel_oscli (const char *cmd) { unsigned long r[10] = { (unsigned long) cmd }; return run (5, r) ? _kernel_ERROR : 0; }
int _kernel_system (const char *cmd, int chain) { (void) chain; return _kernel_oscli (cmd); }
_kernel_oserror *_kernel_getenv (const char *name, char *buf, unsigned size)
{
  unsigned long r[10] = { (unsigned long) name, (unsigned long) buf, size, 0, 0 };		/* OS_ReadVarVal 0x23: R0 name, R1 buffer, R2 size, R3 context, R4 type */
  if (swi_x (0x23, r)) return errorp (r[0]);
  if (r[2] < size) buf[r[2]] = 0; else buf[size - 1] = 0;
  return NULL;
}
_kernel_oserror *_kernel_setenv (const char *name, const char *value)
{
  unsigned long r[10] = { (unsigned long) name, (unsigned long) value, value ? strlen (value) : 0, 0, 0 };		/* OS_SetVarVal 0x24; type 0 = string; R2 = -1 deletes */
  if (!value) r[2] = (unsigned long) -1;
  if (swi_x (0x24, r)) return errorp (r[0]);
  return NULL;
}

/* ---- _swi and _swix (swis.h): the variadic arguments are, in this order: the input registers r0 to r9 as the mask bits 0 to 9 say, the pointers for the output registers
   (bit 31 is r0 ... bit 22 is r9), a pointer for the flags if bit 21 is set, then (bit 11) a block of values (at most 16 here) that is passed by address in the register of
   bits 12 to 15.  _swi returns the register of bits 16 to 19 (the flags if 15: not available here, 0) and ends the program with a message on an error; _swix returns the error. ---- */
#include <stdarg.h>
#include <stdio.h>
_kernel_oserror *_swix (int swi_no, unsigned int mask, ...)
{
  unsigned long r[10] = { 0 };
  va_list ap;
  int i;
  int *outp[10] = { 0 }; unsigned int *flagsp = 0;
  unsigned int block[16]; int have_block = (mask >> 11) & 1;
  va_start (ap, mask);
  for (i = 0; i < 10; i++) if (mask & (1u << i)) r[i] = (unsigned int) va_arg (ap, int);
  for (i = 0; i < 10; i++) if (mask & (1u << (31 - i))) outp[i] = va_arg (ap, int *);
  if (mask & (1u << 21)) flagsp = va_arg (ap, unsigned int *);
  if (have_block) { for (i = 0; i < 16; i++) block[i] = va_arg (ap, unsigned int); r[(mask >> 12) & 15 & 7] = (unsigned long) block; }
  va_end (ap);
  if (swi_x ((unsigned long) ((unsigned) swi_no & 0xFFFFF), r)) return errorp (r[0]);
  for (i = 0; i < 10; i++) if (outp[i]) *outp[i] = (int) r[i];
  if (flagsp) *flagsp = 0;
  last_error.errnum = 0; last_error.errmess[0] = 0;
  return NULL;
}
int _swi (int swi_no, unsigned int mask, ...)
{
  unsigned long r[10] = { 0 };
  va_list ap;
  int i, ret = (int) ((mask >> 16) & 15);
  int *outp[10] = { 0 }; unsigned int *flagsp = 0;
  unsigned int block[16]; int have_block = (mask >> 11) & 1;
  va_start (ap, mask);
  for (i = 0; i < 10; i++) if (mask & (1u << i)) r[i] = (unsigned int) va_arg (ap, int);
  for (i = 0; i < 10; i++) if (mask & (1u << (31 - i))) outp[i] = va_arg (ap, int *);
  if (mask & (1u << 21)) flagsp = va_arg (ap, unsigned int *);
  if (have_block) { for (i = 0; i < 16; i++) block[i] = va_arg (ap, unsigned int); r[(mask >> 12) & 15 & 7] = (unsigned long) block; }
  va_end (ap);
  if (swi_x ((unsigned long) ((unsigned) swi_no & 0xFFFFF), r)) { _kernel_oserror *e = errorp (r[0]); fprintf (stderr, "Error: %s (&%X)\n", e->errmess, (unsigned) e->errnum); exit (1); }
  for (i = 0; i < 10; i++) if (outp[i]) *outp[i] = (int) r[i];
  if (flagsp) *flagsp = 0;
  return ret == 15 ? 0 : (int) r[ret];
}
