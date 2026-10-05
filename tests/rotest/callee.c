/* ABI test callees -- see callee.h.  Keep the arithmetic exact (small dyadic values). */
#include <stdarg.h>
#include "callee.h"

double abi_mix1(int a, double b, float c, long long d, double e, signed char f, short g, double h)
{
    return a + b + c + (double)d + e + f + g + h;
}

double abi_many(double d0, double d1, double d2, double d3, double d4, double d5, double d6, double d7,
                double d8, float f9, int i0, int i1, int i2, int i3, int i4, double d10)
{
    return d0 + d1 * 2 + d2 * 3 + d3 * 4 + d4 * 5 + d5 * 6 + d6 * 7 + d7 * 8 + d8 * 9 + f9 * 10
           + i0 * 11 + i1 * 12 + i2 * 13 + i3 * 14 + i4 * 15 + d10 * 16;
}

D2 abi_d2(D2 a, D2 b)
{
    D2 r = { a.x + b.y, a.y * b.x };
    return r;
}

F4 abi_f4(F4 a, float s)
{
    F4 r = { a.a * s + 1, a.b * s + 2, a.c * s + 3, a.d * s + 4 };
    return r;
}

F5 abi_f5(F5 a, F5 b)
{
    F5 r = { a.a + b.a, a.b + b.b, a.c + b.c, a.d + b.d, a.e + b.e };
    return r;
}

ID abi_id(ID a, int k)
{
    ID r = { a.i + k, a.d * k };
    return r;
}

C7 abi_c7(C7 a)
{
    C7 r;
    for (int i = 0; i < 7; i++)
        r.c[i] = a.c[6 - i];
    return r;
}

LL3 abi_ll3(LL3 a, LL3 b)
{
    LL3 r = { a.a + b.b, a.b - b.a, a.c ^ b.c };
    return r;
}

D3 abi_d3(D3 a, double s)
{
    D3 r = { { a.a[0] * s, a.a[1] + s, a.a[2] - s } };
    return r;
}

BIG abi_big(BIG a, int k)
{
    for (int i = 0; i < 16; i++)
        a.v[i] += k * i;
    return a;
}

SMALL abi_small(SMALL a, SMALL b)
{
    SMALL r = { (signed char)(a.a + b.a), (short)(a.b - b.b), a.c * b.c };
    return r;
}

int abi_narrow(signed char a, unsigned char b, short c, unsigned short d, _Bool e)
{
    return a * 1000000 + b * 10000 + c * 100 + d + (e ? 7 : 0);
}

long long abi_ll(long long a, long long b)
{
    return a * 3 - b;
}

unsigned long long abi_skew(int skew, unsigned long long a, unsigned long long b)
{
    return a * 5 + b + (unsigned)skew;
}

float abi_fret(float a, float b)
{
    return a * b + 0.5f;
}

double abi_vl(const char *types, va_list ap)
{
    double s = 0;
    for (; *types; types++) {
        switch (*types) {
        case 'i': s += va_arg(ap, int); break;
        case 'c': s += va_arg(ap, int); break;
        case 'u': s += (double)va_arg(ap, unsigned); break;
        case 'l': s += (double)va_arg(ap, long long); break;
        case 'd': s += va_arg(ap, double); break;
        case 'p': s += (va_arg(ap, void *) != 0); break;
        default: break;
        }
    }
    return s;
}

double abi_va(const char *types, ...)
{
    va_list ap;
    va_start(ap, types);
    double r = abi_vl(types, ap);
    va_end(ap);
    return r;
}

dcplx abi_cplx(dcplx a, dcplx b)
{
    return a * b + 1.0;
}

int abi_cb(binop_t f, int a, int b)
{
    return f(a, b) + f(b, a) * 2;
}

void abi_out(int *i, double *d, long long *ll, float *f)
{
    *i = 123456;
    *d = 2.5;
    *ll = -9876543210LL;
    *f = 0.25f;
}
