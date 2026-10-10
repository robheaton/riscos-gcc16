/* m_fabs.c - fabsf, copysignf (fabs and copysign are fdlibm's: fd_s_fabs.c, fd_s_copysign.c), fmin, fmax, fdim (and the float ones), nan */
#pragma GCC optimize ("Os")
#include "mathpriv.h"
extern unsigned long long strtoull (const char *s, char **end, int base);

float fabsf (float x) { return m_flt (m_fbits (x) & 0x7FFFFFFFu); }
float copysignf (float x, float y) { return m_flt ((m_fbits (x) & 0x7FFFFFFFu) | (m_fbits (y) & 0x80000000u)); }
/* a quiet NaN is no number: the other one is the answer (both NaN: NaN); a signaling NaN gives a NaN (C23); when the two are equal (+0 and -0 are) the second one is the answer, as the libm of
   glibc on the PC (the SSE instructions) does it */
static int snan_d (double x) { uint64_t u = m_bits (x); return (u & M_EXPMASK) == M_EXPMASK && (u & M_FRAC64) && !(u & 0x0008000000000000ULL); }
static int snan_f (float x) { uint32_t u = m_fbits (x); return (u & 0x7F800000u) == 0x7F800000u && (u & 0x7FFFFFu) && !(u & 0x400000u); }
double fmax (double x, double y)
{
  if (snan_d (x) || snan_d (y)) return x + y;
  if (x != x) return y;
  if (y != y) return x;
  return x > y ? x : y;
}
double fmin (double x, double y)
{
  if (snan_d (x) || snan_d (y)) return x + y;
  if (x != x) return y;
  if (y != y) return x;
  return x < y ? x : y;
}
float fmaxf (float x, float y)
{
  if (snan_f (x) || snan_f (y)) return x + y;
  return (float) fmax (x, y);
}
float fminf (float x, float y)
{
  if (snan_f (x) || snan_f (y)) return x + y;
  return (float) fmin (x, y);
}
double fdim (double x, double y)
{
  double r;
  if (x != x || y != y) return x + y;
  if (x <= y) return 0.0;
  r = x - y;
  if (isinf (r) && !isinf (x) && !isinf (y)) errno = ERANGE;
  return r;
}
/* nan ("123") is a quiet NaN with the payload 123 (the tag is read as strtoull does, base 0), as glibc's */
double nan (const char *tag)
{
  unsigned long long payload = 0;
  const char *p = tag;
  char *end;
  if (*p >= '0' && *p <= '9')
    {
      int saved = errno;                                                       /* (a tag that is too big is not an error of nan) */
      payload = strtoull (p, &end, 0);
      errno = saved;
      if (*end) payload = 0;
    }
  return m_dbl (M_EXPMASK | 0x0008000000000000ULL | (payload & 0x0007FFFFFFFFFFFFULL));
}
float fdimf (float x, float y) { double d = fdim (x, y); return m_fres (d, isfinite (x) && isfinite (y)); }
