/* fpu_control.h - the floating point control register (AArch64 FPCR; after glibc's header) */
#ifndef _FPU_CONTROL_H
#define _FPU_CONTROL_H
#include <stdint.h>
typedef unsigned int fpu_control_t;
#define _FPU_RESERVED 0xfe0fe0ff		/* the bits that must not be changed */
#define _FPU_DEFAULT  0x00000000		/* round to nearest, no traps, no flush to zero */
#define _FPU_IEEE     0x00001f00		/* all the exceptions trap */
#define _FPU_MASK_IOE (1 << 8)			/* invalid operation */
#define _FPU_MASK_DZE (1 << 9)			/* division by zero */
#define _FPU_MASK_OFE (1 << 10)			/* overflow */
#define _FPU_MASK_UFE (1 << 11)			/* underflow */
#define _FPU_MASK_IXE (1 << 12)			/* inexact */
#define _FPU_MASK_IDE (1 << 15)			/* input denormal */
#define _FPU_FLUSH_TZ (1 << 24)
#define _FPU_RC_NEAREST 0x000000
#define _FPU_RC_ZERO    0xc00000
#define _FPU_RC_UP      0x400000
#define _FPU_RC_DOWN    0x800000
#define _FPU_GETCW(cw) do { uint64_t __r; __asm__ volatile ("mrs %0, fpcr" : "=r" (__r)); (cw) = (fpu_control_t) __r; } while (0)
#define _FPU_SETCW(cw) do { uint64_t __r = (cw); __asm__ volatile ("msr fpcr, %0" : : "r" (__r)); } while (0)
extern fpu_control_t __fpu_control;
#endif
