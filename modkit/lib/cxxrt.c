/* cxxrt.c - what C++ code in a module needs at run time (no libstdc++, no exceptions, no RTTI: compile with  -fno-exceptions -fno-rtti  and, best,  -fno-threadsafe-statics ).
     static constructors and destructors   __modlib_cxx_init () calls the functions that the compiler put in .init_array (the linker script, module.ld, collects them between __init_array_start and
                                           __init_array_end), __modlib_cxx_fini () runs the destructors that __aeabi_atexit / __cxa_atexit registered (last in, first out) and then .fini_array.
                                           cmunge makes the module's initialisation and finalisation veneers call them when the CMHG file says  module-is-c-plus-plus: ; otherwise call them yourself
                                           from your initialisation and finalisation code.
     operator new, new[], delete, delete[]   (and the nothrow and sized ones: cxxnew.c, cxxnewa.c, cxxdel.c, cxxdela.c) on malloc / free (the RMA); a failure that new would throw for is abort () (a RISC OS
                                           error in a module).  A program may replace the global operator new (size_t) and operator delete (void *): new[] and the nothrow forms of new go through the
                                           operator new that is linked, and delete[] and the sized and nothrow forms through the operator delete, as in C++.
     __cxa_pure_virtual, the guards of local statics, __dso_handle, and the  std::__throw_*  functions that the headers of libstdc++ call from their templates (<vector>, <array>, <algorithm> ... work as far as
     they need nothing from the library; <string> is instantiated by the code of the module, <map> <set> <list> use members of libstdc++ that the kit links from libstdcxx-mod.a, <iostream> does not work).
   The names are those of the C++ ABI of the ARM (_Znwj is operator new (unsigned)); this file is C, so that the library needs no C++ compiler. */
#pragma GCC optimize ("Os")
#include <stddef.h>
#include <stdlib.h>

extern void (*__init_array_start[]) (void);
extern void (*__init_array_end[]) (void);
extern void (*__fini_array_start[]) (void);
extern void (*__fini_array_end[]) (void);

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
int __aeabi_atexit (void *arg, void (*fn) (void *), void *dso) { return __cxa_atexit (fn, arg, dso); }
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

/* operator new and delete are in cxxnew.c, cxxnewa.c, cxxdel.c and cxxdela.c: one object each, so that a program can replace them */

/* ---- the rest ---- */
void __cxa_pure_virtual (void) { abort (); }
void __cxa_deleted_virtual (void) { abort (); }
int __cxa_guard_acquire (int *g) { return !(*g & 1); }                        /* the guard of a local static, ARM ABI: bit 0 is "done"; one thread of control */
void __cxa_guard_release (int *g) { *g = 1; }
void __cxa_guard_abort (int *g) { (void) g; }
/* std::__throw_bad_alloc () and the others: there are no exceptions, so they end the module's call */
void _ZSt17__throw_bad_allocv (void) { abort (); }
void _ZSt28__throw_bad_array_new_lengthv (void) { abort (); }
void _ZSt16__throw_bad_castv (void) { abort (); }
void _ZSt25__throw_bad_function_callv (void) { abort (); }
void _ZSt20__throw_length_errorPKc (const char *s) { (void) s; abort (); }
void _ZSt19__throw_logic_errorPKc (const char *s) { (void) s; abort (); }
void _ZSt20__throw_out_of_rangePKc (const char *s) { (void) s; abort (); }
void _ZSt24__throw_out_of_range_fmtPKcz (const char *s, ...) { (void) s; abort (); }
void _ZSt21__throw_runtime_errorPKc (const char *s) { (void) s; abort (); }
void _ZSt24__throw_invalid_argumentPKc (const char *s) { (void) s; abort (); }
void _ZSt20__throw_domain_errorPKc (const char *s) { (void) s; abort (); }
void _ZSt21__throw_overflow_errorPKc (const char *s) { (void) s; abort (); }
