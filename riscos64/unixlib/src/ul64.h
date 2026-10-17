/* ul64.h - what the ported UnixLib sources (riscosify.c, unixify.c ...) need from the rest of UnixLib: the SWI calls with 64-bit registers, a few constants, and the declarations.
   (UnixLib 64, riscos64/unixlib; the sources are those of UnixLib, revised BSD licence.)  */
#ifndef UL64_H
#define UL64_H

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unixlib/local.h>

#define MAXPATHLEN 1024
#define __set_errno(e) (errno = (e))
#define stricmp strcasecmp

/* SWI numbers (without the X bit) */
#define OS_File 0x08
#define OS_ReadVarVal 0x23
#define MimeMap_Translate 0x50B00
/* the MimeMap_Translate types */
#define MMM_TYPE_RISCOS               0
#define MMM_TYPE_RISCOS_STRING        1
#define MMM_TYPE_MIME                 2
#define MMM_TYPE_DOT_EXTN             3

/* The SWI veneer of the SDK: x0-x9 in and out, the X bit set; the result is 1 if V was set (then r[0] is the error block).  */
extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);

typedef struct { long r[10]; } _kernel_swi_regs;

/* the error block, or NULL; the registers are 64 bits, so that addresses fit */
static inline const void *__os_swi (unsigned swi, long *regs)
{
  return swi_x (swi, (unsigned long *) regs) ? (const void *) regs[0] : NULL;
}
static inline const void *_kernel_swi (unsigned swi, _kernel_swi_regs *in, _kernel_swi_regs *out)
{
  long r[10];
  const void *e;
  int i;
  for (i = 0; i < 10; i++) r[i] = in->r[i];
  e = __os_swi (swi, r);
  for (i = 0; i < 10; i++) out->r[i] = r[i];
  return e;
}
/* OS_File 17: the type of the object (0 = not found, 1 = file, 2 = directory ...); 0 if the call worked */
static inline int SWI_OS_File_ReadCatInfo (const char *name, int *objtype, unsigned *load, unsigned *exec, unsigned *length, unsigned *attr)
{
  long r[10] = { 17, (long) name };
  if (__os_swi (OS_File, r)) return -1;
  if (objtype) *objtype = (int) r[0];
  if (load) *load = (unsigned) r[2];
  if (exec) *exec = (unsigned) r[3];
  if (length) *length = (unsigned) r[4];
  if (attr) *attr = (unsigned) r[5];
  return 0;
}

/* the home directory of the user (UnixLib: pw_dir of the default user): $HOME, else <Choices$Write> (RISC OS variables as getenv () sees them) */
static inline const char *__ul64_home (void)
{
  const char *h = getenv ("HOME");
  if (!h || !*h) h = getenv ("Choices$Write");
  return h && *h ? h : "<Choices$Write>";
}

extern const char __filename_char_map[256];
extern const char *__sfixfind (const char *sfix, size_t len);
extern void __sdirinit (void);

#endif
