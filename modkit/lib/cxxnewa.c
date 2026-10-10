/* cxxnewa.c - operator new[] (size_t) and the nothrow forms of operator new and new[].  The array form calls the scalar operator new that is linked, as in C++, so that a replacement of operator new covers it;
   the nothrow forms take memory from malloc directly and give a null pointer for a failure instead of abort () (a program that replaces operator new and uses nothrow new replaces those two as well). */
#include <stdlib.h>
extern void *_Znwj (unsigned n);
const char _ZSt7nothrow = 0;                                                 /* std::nothrow (an empty object: libstdc++'s is not in libstdcxx-mod.a) */
void *_Znaj (unsigned n) { return _Znwj (n); }                                /* operator new[] (size_t) */
void *_ZnwjRKSt9nothrow_t (unsigned n, const void *nt) { (void) nt; return malloc (n ? n : 1); }
void *_ZnajRKSt9nothrow_t (unsigned n, const void *nt) { (void) nt; return malloc (n ? n : 1); }
