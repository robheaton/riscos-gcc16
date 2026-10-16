/* modkit64.h - the C interface of a 64-bit module made with the kit (aarch64-riscos-gcc -mmodule).  The module's C code implements some of the entry functions below (the kit has empty defaults for
   all of them) and the module's header file (what cmunge will write: see examples/hello64/hello64_hdr.S) has the tables.  There is no C library in a module (yet): this header has the few things a
   module needs, and a small printf.  Every symbol is hidden (the module is linked as one image that moves as a whole). */
#ifndef MODKIT64_H
#define MODKIT64_H
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <kernel.h>			/* modkit/include: _kernel_oserror, _kernel_swi_regs ... (aarch64-riscos-gcc -mmodule puts that folder on the include path) */
#ifdef __cplusplus
extern "C" {
#endif
#pragma GCC visibility push(hidden)

/* the slots of the register block of a SWI handler: 32 bits on RISC OS Pyromaniac (and in riscos64-clib), 64 bits for a kernel that uses 64-bit slots: -DMODKIT_SLOT=8 */
#ifndef MODKIT_SLOT
#define MODKIT_SLOT 4
#endif
#if MODKIT_SLOT == 8
typedef uint64_t mk_reg;
typedef int64_t mk_sreg;
#else
typedef uint32_t mk_reg;
typedef int32_t mk_sreg;
#endif
typedef mk_reg mk_regs[10];			/* r0-r9 of a SWI; the flags word (r10) is not provided */

typedef _kernel_oserror mk_oserr;
#define MK_NOT_KNOWN ((mk_oserr *) -1)		/* returned by module_swi for a SWI that is not the module's own */

/* the entry points: define the ones the module needs.  An error is returned as a pointer (any readable or unreadable memory: the kit copies it to the RMA), 0 for none. */
mk_oserr *module_init (const char *tail, int instance, void *pw);
mk_oserr *module_final (int fatal, int podule, void *pw);
mk_oserr *module_swi (int offset, mk_reg *regs, void *pw);			/* offset = the SWI number - the chunk base; MK_NOT_KNOWN for one that is not ours */
mk_oserr *module_command (int command, const char *args, int argc, void *pw);	/* command = the number in the command table of the header */
void      module_service (int service, mk_reg *regs, void *pw);		/* a service call (the ones the header's table lists, or all of them if it has no table): the registers r0-r9 as slots; changes are given back (regs[1] = 0 claims it) */

/* the SWI decoding code of the header (swi-decoding-code: in a CMHG file): either one function, module_swi_decode, that gets r0-r3 as the kernel gives them (r0 < 0: r1 = the whole name with its prefix, answer
   the offset in r0; otherwise r0 = the offset, r1 = a buffer, r2 = where in it to write the whole name, r3 = the limit; answer the new r2), or the two below (NAME/NUMBER) */
void module_swi_decode (mk_reg *regs, void *pw);
int  module_swi_name (const char *name, void *pw);						/* the offset of the SWI with this whole name (with the prefix), or -1 */
int  module_swi_number (int number, char *buffer, int offset, int limit, void *pw);		/* writes the whole name at buffer + offset; returns the new offset */

/* the kit */
mk_oserr *mk_error (uint32_t num, const char *msg);			/* an error block in the RMA (valid until the next error), for a return from an entry point */
int  mk_swi (uint32_t swi, uint64_t r[10]);				/* a SWI with the X bit set: r[] in and out (x0-x9); returns 1 if V was set (r[0] is then the error block) */
void *mk_rma_claim (size_t n);						/* OS_Module 6; 0 if it failed */
void  mk_rma_free (void *p);						/* OS_Module 7 */
void mk_putch (int c);							/* OS_WriteC; '\n' is OS_NewLine */
void mk_puts (const char *s);
int  mk_printf (const char *fmt, ...);					/* %d %i %u %x %X %c %s %p %%, the flags 0 and -, a width, the length l (not floating point) */
int  mk_vprintf (const char *fmt, va_list ap);
#pragma GCC visibility pop
#ifdef __cplusplus
}
#endif
#endif
