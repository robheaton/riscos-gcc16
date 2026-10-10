/* mathpriv.h - what the m_*.c files share: the words of a double and a float, and the float wrappers.  (fdlibm.h is for the fdlibm sources.) */
#ifndef MODLIB_MATHPRIV_H
#define MODLIB_MATHPRIV_H
#include <errno.h>
#include <stdint.h>
#include <limits.h>
#include <float.h>
#include <math.h>

typedef union { double d; uint64_t u; } m_du;
typedef union { float f; uint32_t u; } m_fu;
static inline uint64_t m_bits (double d) { m_du x; x.d = d; return x.u; }
static inline double m_dbl (uint64_t u) { m_du x; x.u = u; return x.d; }
static inline uint32_t m_fbits (float f) { m_fu x; x.f = f; return x.u; }
static inline float m_flt (uint32_t u) { m_fu x; x.u = u; return x.f; }
#define M_SIGN64 0x8000000000000000ULL
#define M_EXPMASK 0x7FF0000000000000ULL
#define M_FRAC64 0x000FFFFFFFFFFFFFULL

/* a float function that works in double: the result is rounded once; errno for the float's own overflow and underflow to 0 (the double result can be finite and still too big for a float); a subnormal float sets nothing */
static inline float m_fres (double d, int finite_args)
{
  float f = (float) d;
  if (finite_args)
    {
      if (isinf (f) && !isinf (d)) errno = ERANGE;
      else if (f == 0 && d != 0) errno = ERANGE;
    }
  return f;
}
#endif
