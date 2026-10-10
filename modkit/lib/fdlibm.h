/* fdlibm.h - the private header of the fdlibm sources in lib/fd_*.c: Sun's fdlibm 5.3 as GNU Classpath has it (native/fdlibm), taken over without changes of the algorithms.  The sources keep their
   notice (Copyright Sun Microsystems: "Permission to use, copy, modify, and distribute this software is freely granted, provided that this notice is preserved").  What this header does is give them
   the words of a double through a union (the double of the ARM EABI and of the host is little endian), the prototypes of the internal functions, and the C library's own math.h. */
#ifndef MODLIB_FDLIBM_H
#define MODLIB_FDLIBM_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>

/* fdlibm's way of writing C (the sources are Sun's and are kept as they are) draws these warnings; none of them points at a fault */
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Wdangling-else"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunknown-pragmas"

/* the sources shift and add signed numbers as two's complement (the sign bit of a word, a negative exponent): that is what GCC does for -fwrapv */
#pragma GCC optimize ("wrapv")

#define __P(p) p
#define _IEEE_LIBM
#define _XOPEN_MODE
#define __IEEE_LITTLE_ENDIAN

typedef union
{
  double value;
  struct
  {
    uint32_t lsw;
    uint32_t msw;
  } parts;
} ieee_double_shape_type;

#define EXTRACT_WORDS(ix0, ix1, d)                              \
  do {                                                          \
    ieee_double_shape_type ew_u;                                \
    ew_u.value = (d);                                           \
    (ix0) = ew_u.parts.msw;                                     \
    (ix1) = ew_u.parts.lsw;                                     \
  } while (0)
#define GET_HIGH_WORD(i, d)                                     \
  do {                                                          \
    ieee_double_shape_type gh_u;                                \
    gh_u.value = (d);                                           \
    (i) = gh_u.parts.msw;                                       \
  } while (0)
#define GET_LOW_WORD(i, d)                                      \
  do {                                                          \
    ieee_double_shape_type gl_u;                                \
    gl_u.value = (d);                                           \
    (i) = gl_u.parts.lsw;                                       \
  } while (0)
#define INSERT_WORDS(d, ix0, ix1)                               \
  do {                                                          \
    ieee_double_shape_type iw_u;                                \
    iw_u.parts.msw = (ix0);                                     \
    iw_u.parts.lsw = (ix1);                                     \
    (d) = iw_u.value;                                           \
  } while (0)
#define SET_HIGH_WORD(d, v)                                     \
  do {                                                          \
    ieee_double_shape_type sh_u;                                \
    sh_u.value = (d);                                           \
    sh_u.parts.msw = (v);                                       \
    (d) = sh_u.value;                                           \
  } while (0)
#define SET_LOW_WORD(d, v)                                      \
  do {                                                          \
    ieee_double_shape_type sl_u;                                \
    sl_u.value = (d);                                           \
    sl_u.parts.lsw = (v);                                       \
    (d) = sl_u.value;                                           \
  } while (0)

/* the ieee style functions and the kernels that the wrappers (m_*.c) and the other fdlibm sources call */
extern double __ieee754_sqrt (double);
extern double __ieee754_acos (double);
extern double __ieee754_log (double);
extern double __ieee754_asin (double);
extern double __ieee754_atan2 (double, double);
extern double __ieee754_exp (double);
extern double __ieee754_cosh (double);
extern double __ieee754_fmod (double, double);
extern double __ieee754_pow (double, double);
extern double __ieee754_log10 (double);
extern double __ieee754_sinh (double);
extern double __ieee754_hypot (double, double);
extern double __ieee754_remainder (double, double);
extern int32_t __ieee754_rem_pio2 (double, double *);
extern double __kernel_sin (double, double, int);
extern double __kernel_cos (double, double);
extern double __kernel_tan (double, double, int);
extern int __kernel_rem_pio2 (double *, double *, int, int, int, const int *);
/* the fdlibm functions that have a wrapper of their own (they set errno): the fdlibm side is called __fd_NAME */
extern double __fd_sin (double);
extern double __fd_cos (double);
extern double __fd_tan (double);
extern double __fd_expm1 (double);
extern double __fd_log1p (double);
extern double __fd_scalbn (double, int);

#endif
