/* kernel.h - the _kernel_ functions of the Shared C Library and UnixLib (the SWI and OS call veneers), for RISC OS 64 (SDK, crt/posix/kernel.c).
   As in the 64-bit export of the RISC OS 64 C library, the registers of _kernel_swi_regs are 32 bit values (r[0] ... r[9]): an address that is passed in a register must be
   below 4 GB (a static or stack object of the program is; memory from malloc may not be: copy to a static buffer for the SWIs that read or write it). */
#ifndef __kernel_h
#define __kernel_h
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { int r[10]; } _kernel_swi_regs;
typedef struct { int errnum; char errmess[252]; } _kernel_oserror;
typedef struct { int load, exec, start, end; } _kernel_osfile_block;
typedef struct { void *dataptr; int nbytes, fileptr, buf_len; char *wild_fld; } _kernel_osgbpb_block;
#define _kernel_NONX 0x80000000
#define _kernel_ERROR (-2)
#define _kernel_OSERROR (-2)
_kernel_oserror *_kernel_swi (int, _kernel_swi_regs *, _kernel_swi_regs *);
_kernel_oserror *_kernel_swi_c (int, _kernel_swi_regs *, _kernel_swi_regs *, int *);
int _kernel_osbyte (int, int, int);
int _kernel_osrdch (void);
int _kernel_oswrch (int);
int _kernel_osbget (int);
int _kernel_osbput (int, int);
int _kernel_osgbpb (int, int, _kernel_osgbpb_block *);
int _kernel_osword (int, int *);
int _kernel_osfind (int, char *);
int _kernel_osfile (int, const char *, _kernel_osfile_block *);
int _kernel_osargs (int, int, int);
int _kernel_oscli (const char *);
_kernel_oserror *_kernel_last_oserror (void);
int _kernel_system (const char *, int);
_kernel_oserror *_kernel_getenv (const char *, char *, unsigned);
_kernel_oserror *_kernel_setenv (const char *, const char *);
#ifdef __cplusplus
}
#endif
#endif
