/* cxxrt64.c - what C++ code in a 64-bit module needs at run time (no libstdc++, no exceptions, no RTTI: compile with -fno-exceptions -fno-rtti -fno-threadsafe-statics, which aarch64-riscos-gcc -mmodule does for
   .cc/.cpp/.cxx files).  The AArch64 version of modkit/lib/cxxrt.c, with the names of the Itanium C++ ABI of LP64 (_Znwm is operator new (unsigned long)):
     static constructors and destructors   __modlib_cxx_init () calls the functions of .init_array (module64.ld collects them between __init_array_start and __init_array_end), __modlib_cxx_fini () runs the
                                           destructors that __cxa_atexit registered (last in, first out) and then .fini_array.  cmunge64 makes the module's veneers call them for module-is-c-plus-plus:.
     operator new, new[], delete, delete[] (and the nothrow and sized forms) on malloc / free (cxxheap64.c: the RMA); a failure that new would throw for is abort () (an error of RISC OS).
     __cxa_pure_virtual, the guards of local statics, __dso_handle, std::__throw_* for the headers of libstdc++ that call them. */
#include "modkit64.h"
#pragma GCC visibility push(hidden)
extern void (*__init_array_start[]) (void);
extern void (*__init_array_end[]) (void);
extern void (*__fini_array_start[]) (void);
extern void (*__fini_array_end[]) (void);
extern void *malloc (size_t n);
extern void free (void *p);
extern void abort (void) __attribute__ ((noreturn));

void *__dso_handle = 0;
typedef struct dtor { void (*fn) (void *); void *arg; struct dtor *next; } dtor;
static dtor *dtors;

int __cxa_atexit (void (*fn) (void *), void *arg, void *dso)
{
  dtor *d = malloc (sizeof *d);
  (void) dso;
  if (!d) return -1;
  d->fn = fn; d->arg = arg; d->next = dtors; dtors = d;
  return 0;
}
void __cxa_finalize (void *dso)
{
  (void) dso;
  while (dtors)
    {
      dtor *d = dtors;
      dtors = d->next;
      d->fn (d->arg);
      free (d);
    }
}
void __modlib_cxx_init (void)
{
  void (**f) (void);
  for (f = __init_array_start; f != __init_array_end; f++) (*f) ();
}
void __modlib_cxx_fini (void)
{
  void (**f) (void) = __fini_array_end;
  __cxa_finalize (0);
  while (f != __fini_array_start) (*--f) ();
}

void __cxa_pure_virtual (void) { abort (); }
void __cxa_deleted_virtual (void) { abort (); }
int __cxa_guard_acquire (long long *g) { return !(*(char *) g & 1); }          /* the guard of a local static: the first byte is "done"; one thread of control */
void __cxa_guard_release (long long *g) { *(char *) g = 1; }
void __cxa_guard_abort (long long *g) { (void) g; }

/* operator new / delete: the global ones, the array forms, the sized and the nothrow forms */
void *_Znwm (unsigned long n) { void *p = malloc (n ? n : 1); if (!p) abort (); return p; }
void *_Znam (unsigned long n) { return _Znwm (n); }
void *_ZnwmRKSt9nothrow_t (unsigned long n, const void *nt) { (void) nt; return malloc (n ? n : 1); }
void *_ZnamRKSt9nothrow_t (unsigned long n, const void *nt) { (void) nt; return malloc (n ? n : 1); }
void _ZdlPv (void *p) { free (p); }
void _ZdaPv (void *p) { _ZdlPv (p); }
void _ZdlPvm (void *p, unsigned long n) { (void) n; _ZdlPv (p); }
void _ZdaPvm (void *p, unsigned long n) { (void) n; _ZdlPv (p); }
void _ZdlPvRKSt9nothrow_t (void *p, const void *nt) { (void) nt; _ZdlPv (p); }
void _ZdaPvRKSt9nothrow_t (void *p, const void *nt) { (void) nt; _ZdlPv (p); }
const char _ZSt7nothrow = 0;

/* std::__throw_bad_alloc () and the others: there are no exceptions, so they end the module's call */
void _ZSt17__throw_bad_allocv (void) { abort (); }
void _ZSt28__throw_bad_array_new_lengthv (void) { abort (); }
void _ZSt16__throw_bad_castv (void) { abort (); }
void _ZSt25__throw_bad_function_callv (void) { abort (); }
void _ZSt20__throw_length_errorPKc (const char *s) { (void) s; abort (); }
void _ZSt19__throw_logic_errorPKc (const char *s) { (void) s; abort (); }
void _ZSt20__throw_out_of_rangePKc (const char *s) { (void) s; abort (); }
void _ZSt21__throw_runtime_errorPKc (const char *s) { (void) s; abort (); }
void _ZSt24__throw_invalid_argumentPKc (const char *s) { (void) s; abort (); }
