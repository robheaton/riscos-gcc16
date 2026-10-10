/* cxxmod.cc - a module written in C++.  What it shows: static objects with constructors and destructors (run by the module's initialisation and finalisation, because the CMHG file says
   module-is-c-plus-plus:), classes with virtual functions, new and delete, a template, a local static, a lambda.  Build: see the Makefile (module.mk compiles .cc files with
   g++ -mmodule -fno-exceptions -fno-rtti -fno-threadsafe-statics and links with the C driver: there is no libstdc++ in a module, lib/cxxrt.c of the kit has operator new and the rest). */
#include <new>
#include <stdlib.h>
#include <stdio.h>
#include <kernel.h>
#include "header.h"

namespace
{
  struct Counter
  {
    int constructed;
    Counter () : constructed (1) { printf ("CxxMod: the static object Counter is constructed\n"); }
    ~Counter () { printf ("CxxMod: the static object Counter is destroyed (%d shapes were made)\n", shapes_made); }
    static int shapes_made;
  };
  int Counter::shapes_made = 0;
  Counter counter;                                                  // runs before cxx_init

  struct Shape
  {
    Shape () { Counter::shapes_made++; }
    virtual ~Shape () {}
    virtual int area () const = 0;
  };
  struct Square : Shape
  {
    int s;
    explicit Square (int v) : s (v) {}
    int area () const override { return s * s; }
  };
  struct Rect : Shape
  {
    int w, h;
    Rect (int a, int b) : w (a), h (b) {}
    int area () const override { return w * h; }
  };

  template <class R, class S> R sum_areas (S *const *p, int n)
  {
    R total = 0;
    for (int i = 0; i < n; i++) total += p[i]->area ();
    return total;
  }
  int calls ()
  {
    static int n = 0;                                               // a local static
    return ++n;
  }
}

extern "C" _kernel_oserror *cxx_init (const char *tail, int podule_base, void *pw)
{
  (void) tail; (void) podule_base; (void) pw;
  printf ("CxxMod: initialisation code (Counter.constructed = %d)\n", counter.constructed);
  return 0;
}
extern "C" _kernel_oserror *cxx_final (int fatal, int podule_base, void *pw)
{
  (void) fatal; (void) podule_base; (void) pw;
  printf ("CxxMod: finalisation code\n");
  return 0;
}
extern "C" _kernel_oserror *cxx_command (const char *arg_string, int argc, int number, void *pw)
{
  (void) pw;
  switch (number)
    {
    case CMD_CxxMod_Run:
      {
        int n = argc ? atoi (arg_string) : 3;
        if (n < 1 || n > 100) n = 3;
        Shape **shapes = new Shape *[n];
        for (int i = 0; i < n; i++) shapes[i] = (i & 1) ? static_cast<Shape *> (new Rect (i + 1, 2)) : static_cast<Shape *> (new Square (i + 1));
        auto total = [&] () { int t = 0; for (int i = 0; i < n; i++) t += shapes[i]->area (); return t; };
        printf ("CxxMod_Run %d: %d shapes, total area %d (%d), call %d\n", n, n, total (), sum_areas<int> (shapes, n), calls ());
        for (int i = 0; i < n; i++) delete shapes[i];
        delete[] shapes;
        break;
      }
    case CMD_CxxMod_Info:
      printf ("CxxMod_Info: Counter.constructed = %d, shapes made so far %d\n", counter.constructed, Counter::shapes_made);
      break;
    }
  return 0;
}
