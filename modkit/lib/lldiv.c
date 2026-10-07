/* lldiv.c - lldiv () on its own: the 64 bit division needs libgcc, so it is not in stdlib.c (a module that does not use it does not need libgcc). */
#include <stdlib.h>
lldiv_t lldiv (long long n, long long d) { lldiv_t r; r.quot = n / d; r.rem = n % d; return r; }
