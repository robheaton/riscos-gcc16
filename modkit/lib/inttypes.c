/* inttypes.c - imaxabs, imaxdiv, strtoimax and strtoumax of <inttypes.h>: intmax_t is long long here, so each is the function of <stdlib.h> that has the type (llabs, lldiv, strtoll, strtoull).  (Real functions, as
   in the module library of GCCSDK 4.7.4: until 16.2.0-22 they were inline functions of the header.) */
#include <inttypes.h>
#include <stdlib.h>

intmax_t imaxabs (intmax_t j) { return llabs (j); }
imaxdiv_t imaxdiv (intmax_t n, intmax_t d)
{
  lldiv_t q = lldiv (n, d);
  imaxdiv_t r;
  r.quot = q.quot; r.rem = q.rem;
  return r;
}
intmax_t strtoimax (const char *s, char **end, int base) { return strtoll (s, end, base); }
uintmax_t strtoumax (const char *s, char **end, int base) { return strtoull (s, end, base); }
