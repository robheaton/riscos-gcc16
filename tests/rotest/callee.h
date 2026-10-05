/* Functions implemented in callee.c.  The test driver can be compiled by a different
   compiler (or version) than callee.c, which checks the calling convention end to end:
   register vs stack args, 8-byte alignment of 64-bit args, VFP homogeneous aggregates,
   struct returns, narrow-integer extension, variadics and va_list hand-over. */
#ifndef CALLEE_H
#define CALLEE_H
#include <stdarg.h>

typedef struct { double x, y; } D2;                 /* HFA of 2 doubles */
typedef struct { float a, b, c, d; } F4;            /* HFA of 4 floats */
typedef struct { float a, b, c, d, e; } F5;         /* 5 floats: not an HFA */
typedef struct { int i; double d; } ID;             /* mixed, 16 bytes, 8-aligned */
typedef struct { char c[7]; } C7;                   /* odd size */
typedef struct { long long a, b; int c; } LL3;      /* 24 bytes, 8-aligned */
typedef struct { double a[3]; } D3;                 /* HFA via array */
typedef struct { int v[16]; } BIG;                  /* 64 bytes by value */
typedef struct { signed char a; short b; int c; } SMALL;
typedef double _Complex dcplx;
typedef int (*binop_t)(int, int);

double    abi_mix1(int a, double b, float c, long long d, double e, signed char f, short g, double h);
double    abi_many(double d0, double d1, double d2, double d3, double d4, double d5, double d6, double d7,
                   double d8, float f9, int i0, int i1, int i2, int i3, int i4, double d10);
D2        abi_d2(D2 a, D2 b);
F4        abi_f4(F4 a, float s);
F5        abi_f5(F5 a, F5 b);
ID        abi_id(ID a, int k);
C7        abi_c7(C7 a);
LL3       abi_ll3(LL3 a, LL3 b);
D3        abi_d3(D3 a, double s);
BIG       abi_big(BIG a, int k);
SMALL     abi_small(SMALL a, SMALL b);
int       abi_narrow(signed char a, unsigned char b, short c, unsigned short d, _Bool e);
long long abi_ll(long long a, long long b);
unsigned long long abi_skew(int skew, unsigned long long a, unsigned long long b);
float     abi_fret(float a, float b);
double    abi_va(const char *types, ...);
double    abi_vl(const char *types, va_list ap);
dcplx     abi_cplx(dcplx a, dcplx b);
int       abi_cb(binop_t f, int a, int b);
void      abi_out(int *i, double *d, long long *ll, float *f);
#endif
