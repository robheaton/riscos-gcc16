/* cside.c -- the C half of the Fortran/C interoperability test (fcinterop.f90). */
#include <string.h>

int c_add (int a, int b) { return a + b; }

double c_dot (const double *x, const double *y, int n)
{
  double s = 0.0;
  for (int i = 0; i < n; i++) s += x[i] * y[i];
  return s;
}

void c_fill (int *a, int n) { for (int i = 0; i < n; i++) a[i] = i * i; }

int c_strlen (const char *s) { return (int) strlen (s); }

void c_upcase (char *s) { for (; *s; s++) if (*s >= 'a' && *s <= 'z') *s -= 32; }

/* calls back into Fortran (a bind(c) procedure) n times */
void c_each (void (*f) (int), int n) { for (int i = 1; i <= n; i++) f (i); }

struct pair { int a; double b; };
double c_pair_sum (const struct pair *p) { return p->a + p->b; }

int c_counter = 100;                   /* a C global used from Fortran */
