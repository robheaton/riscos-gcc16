/* kernel.h - the part of SharedCLibrary's <kernel.h> that a module needs: the error block, the register block, _kernel_swi.  (modkit: modules are built without UnixLib or the Shared C Library.) */
#ifndef __KERNEL_H
#define __KERNEL_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>						/* (size_t: the SharedCLibrary's kernel.h defines it too) */
typedef struct { int errnum; char errmess[252]; } _kernel_oserror;
typedef struct { int r[10]; } _kernel_swi_regs;
typedef struct stack_chunk { unsigned long sc_mark; struct stack_chunk *sc_next, *sc_prev; unsigned long sc_size; int (*sc_deallocate) (); } _kernel_stack_chunk;      /* (the type only: a module has no stack chunks of the SharedCLibrary) */
#define _kernel_NONX 0x80000000
/* Call the SWI NO with the registers IN, return the registers in OUT.  The X bit is set unless bit 31 (_kernel_NONX) is set. */
/* The bytes that are left of the kernel's SVC stack (32 KB, shared with the OS) for the caller, or -1 when that cannot be known (user mode, the stack of a runnable module).  There is no overflow
   check: a module that recurses or has big local arrays (more than a few KB) should ask first, or use malloc. */
extern long __modlib_stack_left (void);
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
/* interrupts and the processor mode (ARMv6: CPSIE / CPSID; in USR mode they change nothing, as in the SharedCLibrary) */
extern void _kernel_irqs_on (void);
extern void _kernel_irqs_off (void);
extern int _kernel_irqs_disabled (void);                         /* 0 when IRQs are enabled, else not 0 (the I bit) */
extern int _kernel_processor_mode (void);                        /* the mode bits of the CPSR: 0x10 USR, 0x12 IRQ, 0x13 SVC */
/* blocks of the RMA, straight from OS_Module (no header of the library's: use these with each other, not with free ()) */
extern void *_kernel_RMAalloc (size_t size);                     /* NULL when there is no room (or size is 0) */
extern void *_kernel_RMAextend (void *p, size_t size);           /* the block becomes SIZE bytes (p NULL: as _kernel_RMAalloc, size 0: as _kernel_RMAfree); NULL on error: the old block is still there */
extern void _kernel_RMAfree (void *p);
#ifdef __cplusplus
}
#endif
#endif
