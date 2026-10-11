/* kos.c - the OS calls of the kernel interface: _kernel_osbyte, osrdch, oswrch, osbget, osbput, osfile, osgbpb, osword, osfind, osargs, oscli. */
#include <stddef.h>
#include <stdarg.h>
#include <kernel.h>
#include <swis.h>
#include "hostptr.h"

extern _kernel_oserror *__modlib_xswi (unsigned swi_x, unsigned *regs);
extern _kernel_oserror *__modlib_xswif (unsigned swi_x, unsigned *regs, unsigned *flags);
extern _kernel_oserror *__modlib_remember (const _kernel_oserror *e);

#define CARRY	0x20000000u
#define X	0x20000u
#define remember __modlib_remember

int _kernel_osbyte (int op, int x, int y)
{
  unsigned r[10] = { (unsigned) op, (unsigned) x, (unsigned) y }, f = 0;
  _kernel_oserror *e = __modlib_xswif (0x06 | X, r, &f);
  if (e) { remember (e); return _kernel_ERROR; }
  return (int) ((r[1] & 0xFF) | ((r[2] & 0xFF) << 8) | ((f & CARRY) ? 1u << 16 : 0));
}
int _kernel_osrdch (void)
{
  unsigned r[10] = { 0 }, f = 0;
  _kernel_oserror *e = __modlib_xswif (0x04 | X, r, &f);
  if (e) { remember (e); return _kernel_ERROR; }
  if (f & CARRY) return r[0] == 27 ? -27 : -1;
  return (int) r[0];
}
int _kernel_oswrch (int ch)
{
  unsigned r[10] = { (unsigned) ch };
  _kernel_oserror *e = __modlib_xswi (0x00 | X, r);
  if (e) { remember (e); return _kernel_ERROR; }
  return 0;
}
int _kernel_osbget (unsigned handle)
{
  unsigned r[10] = { 0, handle }, f = 0;
  _kernel_oserror *e = __modlib_xswif (0x0A | X, r, &f);
  if (e) { remember (e); return _kernel_ERROR; }
  return (f & CARRY) ? -1 : (int) r[0];
}
int _kernel_osbput (int ch, unsigned handle)
{
  unsigned r[10] = { (unsigned) ch, handle };
  _kernel_oserror *e = __modlib_xswi (0x0B | X, r);
  if (e) { remember (e); return _kernel_ERROR; }
  return 0;
}
int _kernel_osfile (int op, const char *name, _kernel_osfile_block *b)
{
  unsigned r[10] = { (unsigned) op, PIN (name), (unsigned) b->load, (unsigned) b->exec, (unsigned) b->start, (unsigned) b->end };
  _kernel_oserror *e = __modlib_xswi (0x08 | X, r);
  b->load = (int) r[2]; b->exec = (int) r[3]; b->start = (int) r[4]; b->end = (int) r[5];
  if (e) { remember (e); return _kernel_ERROR; }
  return (int) r[0];
}
int _kernel_osgbpb (int op, unsigned handle, _kernel_osgbpb_block *b)
{
  unsigned r[10] = { (unsigned) op, handle, PIN (b->dataptr), (unsigned) b->nbytes, (unsigned) b->fileptr, (unsigned) b->buf_len, PIN (b->wild_fld) }, f = 0;
  _kernel_oserror *e = __modlib_xswif (0x0C | X, r, &f);
  if (e) { remember (e); return _kernel_ERROR; }
  b->nbytes = (int) r[3]; b->fileptr = (int) r[4]; b->buf_len = (int) r[5];
#ifndef MODLIB_HOST                                                                      /* (on the host the registers hold tokens, not pointers) */
  b->dataptr = (void *) r[2]; b->wild_fld = (char *) r[6];
#endif
  return (f & CARRY) ? -1 : (int) r[0];
}
int _kernel_osword (int op, int *data)
{
  unsigned r[10] = { (unsigned) op, PIN (data) };
  _kernel_oserror *e = __modlib_xswi (0x07 | X, r);
  if (e) { remember (e); return _kernel_ERROR; }
  return 0;
}
int _kernel_osfind (int op, const char *name)
{
  unsigned r[10] = { (unsigned) op, op ? PIN (name) : (unsigned) (size_t) name };       /* a close (op 0) has the handle where the name is */
  _kernel_oserror *e = __modlib_xswi (0x0D | X, r);
  if (e) { remember (e); return _kernel_ERROR; }
  return (int) r[0];
}
int _kernel_osargs (int op, unsigned handle, int arg)
{
  unsigned r[10] = { (unsigned) op, handle, (unsigned) arg };
  _kernel_oserror *e = __modlib_xswi (0x09 | X, r);
  if (e) { remember (e); return _kernel_ERROR; }
  return (op | (int) handle) ? (int) r[2] : (int) r[0];
}
int _kernel_oscli (const char *s)
{
  unsigned r[10] = { PIN (s) };
  _kernel_oserror *e = __modlib_xswi (0x05 | X, r);
  if (e) { remember (e); return _kernel_ERROR; }
  return 1;
}

/* ---- a few more of the SharedCLibrary's _kernel_ functions that a module may call ---- */
extern void __modlib_raise (const void *e);

/* Raise the error through the error handler of the caller (OS_GenerateError, the non-X form: it does not come back to a module that is called from the system); the value is 0 when it does. */
int _kernel_raise_error (const _kernel_oserror *e)
{
  __modlib_raise (e);
  return 0;
}

/* 1 when the Escape key was pressed since the last call (and the condition is acknowledged), else 0 (OS_Byte 126). */
int _kernel_escape_seen (void)
{
  int r = _kernel_osbyte (126, 0, 0);
  return r != _kernel_ERROR && (r & 0xFF) != 0;
}

/* There is no floating point hardware in a module (the code is soft float). */
int _kernel_fpavailable (void) { return 0; }

/* The command line of the program: a module has none (a runnable module gets its arguments as argc and argv). */
char *_kernel_command_string (void) { return NULL; }

/* Run a command through the command line interpreter (OS_CLI): the same as system () but with the error and the return value as the SharedCLibrary has them: -2 when the OS reports an error (the
   error is _kernel_last_oserror ()), else 0.  The second argument (CHAIN: the command replaces the program) is not supported: it is treated as 0. */
int _kernel_system (const char *cmd, int chain)
{
  (void) chain;
  return _kernel_oscli (cmd) == _kernel_ERROR ? -2 : 0;
}

_kernel_ExtendProc *_kernel_register_slotextend (_kernel_ExtendProc *handler)
{
  (void) handler;
  return 0;
}
