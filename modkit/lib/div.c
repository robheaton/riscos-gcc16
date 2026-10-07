/* div.c - abs, labs, llabs, div, ldiv (the 32 bit division is the one of divmod.c). */
#include <stdlib.h>
int abs (int v) { return v < 0 ? -v : v; }
long labs (long v) { return v < 0 ? -v : v; }
long long llabs (long long v) { return v < 0 ? -v : v; }
div_t div (int n, int d) { div_t r; r.quot = n / d; r.rem = n % d; return r; }
ldiv_t ldiv (long n, long d) { ldiv_t r; r.quot = n / d; r.rem = n % d; return r; }
