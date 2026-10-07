/* kernel.h - the part of SharedCLibrary's <kernel.h> that a module needs: the error block, the register block, _kernel_swi.  (modkit: modules are built without UnixLib or the Shared C Library.) */
#ifndef __KERNEL_H
#define __KERNEL_H
typedef struct { int errnum; char errmess[252]; } _kernel_oserror;
typedef struct { int r[10]; } _kernel_swi_regs;
#define _kernel_NONX 0x80000000
/* Call the SWI NO with the registers IN, return the registers in OUT.  The X bit is set unless bit 31 (_kernel_NONX) is set. */
extern _kernel_oserror *_kernel_swi (int no, const _kernel_swi_regs *in, _kernel_swi_regs *out);
extern _kernel_oserror *_kernel_swi_c (int no, const _kernel_swi_regs *in, _kernel_swi_regs *out, int *carry);

/* the OS calls of the SharedCLibrary kernel interface.  The calls that return an int: >= 0 is success, -1 is "failed but there is no OS error" (the carry flag: end of file, escape), _kernel_ERROR (-2) is an
   OS error that _kernel_last_oserror () gives (once). */
#define _kernel_ERROR (-2)
extern _kernel_oserror *_kernel_last_oserror (void);
extern int _kernel_osbyte (int op, int x, int y);                /* r1 in the low byte, r2 in the next one, bit 16: carry */
extern int _kernel_osrdch (void);
extern int _kernel_oswrch (int ch);
extern int _kernel_osbget (unsigned handle);
extern int _kernel_osbput (int ch, unsigned handle);
typedef struct { int load, exec, start, end; } _kernel_osfile_block;
extern int _kernel_osfile (int op, const char *name, _kernel_osfile_block *inout);
typedef struct { void *dataptr; int nbytes, fileptr; int buf_len; char *wild_fld; } _kernel_osgbpb_block;
extern int _kernel_osgbpb (int op, unsigned handle, _kernel_osgbpb_block *inout);
extern int _kernel_osword (int op, int *data);
extern int _kernel_osfind (int op, const char *name);
extern int _kernel_osargs (int op, unsigned handle, int arg);
extern int _kernel_oscli (const char *s);                        /* 1 when the command worked */
extern _kernel_oserror *_kernel_getenv (const char *name, char *buffer, unsigned size);        /* the value of a system variable (OS_ReadVarVal) */
extern _kernel_oserror *_kernel_setenv (const char *name, const char *value);                  /* NULL as the value deletes it */
#endif
