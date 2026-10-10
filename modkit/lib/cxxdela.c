/* cxxdela.c - operator delete[] (void *), the sized forms (C++14) and the nothrow forms: they call the operator delete (void *) that is linked, as in C++, so that a replacement of it covers them. */
extern void _ZdlPv (void *p);
void _ZdaPv (void *p) { _ZdlPv (p); }                                         /* operator delete[] (void *) */
void _ZdlPvj (void *p, unsigned n) { (void) n; _ZdlPv (p); }                  /* operator delete (void *, size_t) */
void _ZdaPvj (void *p, unsigned n) { (void) n; _ZdlPv (p); }
void _ZdlPvRKSt9nothrow_t (void *p, const void *nt) { (void) nt; _ZdlPv (p); }
void _ZdaPvRKSt9nothrow_t (void *p, const void *nt) { (void) nt; _ZdlPv (p); }
