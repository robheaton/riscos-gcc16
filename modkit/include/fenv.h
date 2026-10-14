/* fenv.h - the floating point environment of C99, for a target without floating point hardware: the exception flags and the rounding mode are SOFTWARE state.  feraiseexcept, fesetexceptflag, fesetenv and
   feupdateenv set the flags and fetestexcept, fegetexceptflag and fegetenv read them; the arithmetic of the soft float routines and <math.h> do NOT set them (a division by zero leaves FE_DIVBYZERO clear).
   The rounding mode is always FE_TONEAREST (fesetround of another mode fails with a non-zero result).  The values are those of glibc on ARM. */
#ifndef _FENV_H
#define _FENV_H
#ifdef __cplusplus
extern "C" {
#endif
#define FE_INVALID	0x01
#define FE_DIVBYZERO	0x02
#define FE_OVERFLOW	0x04
#define FE_UNDERFLOW	0x08
#define FE_INEXACT	0x10
#define FE_ALL_EXCEPT	(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW | FE_INEXACT)
#define FE_TONEAREST	0
#define FE_UPWARD	0x400000
#define FE_DOWNWARD	0x800000
#define FE_TOWARDZERO	0xC00000
typedef struct { unsigned int __flags; } fenv_t;
typedef unsigned int fexcept_t;
#define FE_DFL_ENV	((const fenv_t *) -1)
extern int feclearexcept (int excepts);
extern int fegetexceptflag (fexcept_t *flagp, int excepts);
extern int feraiseexcept (int excepts);
extern int fesetexceptflag (const fexcept_t *flagp, int excepts);
extern int fetestexcept (int excepts);
extern int fegetround (void);
extern int fesetround (int round);
extern int fegetenv (fenv_t *envp);
extern int feholdexcept (fenv_t *envp);
extern int fesetenv (const fenv_t *envp);
extern int feupdateenv (const fenv_t *envp);
#ifdef __cplusplus
}
#endif
#endif
