// cxx64.cc - a C++ module for RISC OS 64: static constructors and destructors, virtual functions, new and delete, a template and a local static.
// cmunge64.py -d cxx64h.h -o cxx64_hdr.o cxx64.cmhg;  aarch64-riscos-gcc -mmodule -O2 cxx64.cc cxx64_hdr.o -o Cxx64   (no exceptions, no RTTI, no libstdc++)
#include "kernel.h"
#include "modkit64.h"
#include "cxx64h.h"

static int ctor_count, dtor_count, objects_alive;

struct Counter { Counter () { ctor_count++; } ~Counter () { dtor_count++; } };
static Counter counter1, counter2;					// constructed before cxx_init runs, destroyed after cxx_final

struct Bye { ~Bye () { mk_printf ("Cxx64: static destructor ran (%d constructed, %d destroyed before this)\n", ctor_count, dtor_count); } };
static Bye bye;

struct Shape
{
  Shape () { objects_alive++; }
  virtual ~Shape () { objects_alive--; }
  virtual int area () const = 0;
  virtual const char *name () const = 0;
};
struct Rect : Shape
{
  int w, h;
  Rect (int w_, int h_) : w (w_), h (h_) { }
  int area () const override { return w * h; }
  const char *name () const override { return "rectangle"; }
};
struct Square : Rect
{
  explicit Square (int s) : Rect (s, s) { }
  const char *name () const override { return "square"; }
};

template <class T> T twice (T x) { return x + x; }
static int next_id () { static int id = 100; return id++; }		// a local static with a guard

extern "C" _kernel_oserror *cxx_init (const char *tail, int podule_base, void *pw)
{
  mk_printf ("Cxx64: started (%d static objects constructed)\n", ctor_count);
  return 0;
}

extern "C" _kernel_oserror *cxx_final (int fatal, int podule_base, void *pw)
{
  mk_printf ("Cxx64: finishing (%d objects alive)\n", objects_alive);
  return 0;
}

extern "C" _kernel_oserror *cxx_swi (int swi_offset, _kernel_swi_regs *r, void *pw)
{
  if (swi_offset != 0) return error_BAD_SWI;
  Shape *a = new Rect (3, 4), *b = new Square (5);			// operator new, virtual calls
  int sum = a->area () + b->area ();					// 12 + 25
  r->r[0] = ctor_count;							// 2
  r->r[1] = sum;							// 37
  r->r[2] = objects_alive;						// 2 while both are alive
  delete a;
  delete b;
  r->r[3] = objects_alive;						// 0
  r->r[4] = twice<int> (21);						// 42
  r->r[5] = next_id () * 1000 + next_id ();				// 100101
  r->r[6] = dtor_count;							// 0 (the static destructors run at finalisation)
  return 0;
}

extern "C" _kernel_oserror *cxx_command (const char *arg_string, int argc, int number, void *pw)
{
  Shape *s[2] = { new Rect (6, 7), new Square (9) };
  for (int i = 0; i < 2; i++) mk_printf ("%s: area %d\n", s[i]->name (), s[i]->area ());
  delete s[0];
  delete s[1];
  return 0;
}
