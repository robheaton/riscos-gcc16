/* fenv.c - <fenv.h> for a target without floating point hardware: the flags are a word of software state (nothing in the library or in the soft float routines sets them), the rounding mode is
   FE_TONEAREST.  The functions of the Shared C Library that GCCSDK 4.7.4's module library calls are all here, so a source that uses fenv.h builds; see fenv.h for what they do. */
#include <fenv.h>

static unsigned flags;                                     /* the exception flags raised so far */

int feclearexcept (int excepts) { flags &= ~((unsigned) excepts & FE_ALL_EXCEPT); return 0; }
int fegetexceptflag (fexcept_t *flagp, int excepts) { *flagp = flags & (unsigned) excepts & FE_ALL_EXCEPT; return 0; }
int feraiseexcept (int excepts) { flags |= (unsigned) excepts & FE_ALL_EXCEPT; return 0; }
int fesetexceptflag (const fexcept_t *flagp, int excepts) { unsigned m = (unsigned) excepts & FE_ALL_EXCEPT; flags = (flags & ~m) | (*flagp & m); return 0; }
int fetestexcept (int excepts) { return (int) (flags & (unsigned) excepts & FE_ALL_EXCEPT); }
int fegetround (void) { return FE_TONEAREST; }
int fesetround (int round) { return round == FE_TONEAREST ? 0 : 1; }
int fegetenv (fenv_t *envp) { envp->__flags = flags; return 0; }
int feholdexcept (fenv_t *envp) { envp->__flags = flags; flags = 0; return 0; }              /* (no trapping: "non-stop" mode is the only mode) */
int fesetenv (const fenv_t *envp) { flags = envp == FE_DFL_ENV ? 0 : envp->__flags & FE_ALL_EXCEPT; return 0; }
int feupdateenv (const fenv_t *envp) { unsigned raised = flags; fesetenv (envp); flags |= raised; return 0; }
