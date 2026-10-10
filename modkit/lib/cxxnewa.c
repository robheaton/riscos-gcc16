/* cxxnewa.c - operator new[] (size_t) and the nothrow forms of operator new and new[].  The array form calls the scalar operator new that is linked, as in C++, so that a replacement of operator new covers it;
   the nothrow forms give a null pointer for a failure instead of abort (). */
#include <stdlib.h>
extern void *_Znwj (unsigned n);
void *_Znaj (unsigned n) { return _Znwj (n); }                                /* operator new[] (size_t) */
void *_ZnwjRKSt9nothrow_t (unsigned n, const void *nt) { (void) nt; return malloc (n ? n : 1); }
void *_ZnajRKSt9nothrow_t (unsigned n, const void *nt) { (void) nt; return malloc (n ? n : 1); }
