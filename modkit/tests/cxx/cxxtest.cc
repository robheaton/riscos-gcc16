/* cxxtest.cc - C++ in a module: the same program is built for the host (g++, glibc) and for the module target (g++ -mmodule, no libstdc++, no exceptions) and must print the same text.
   Static constructors and destructors (in two translation units), virtual functions, multiple inheritance, abstract classes, templates, new and delete (also the array and placement forms),
   local statics, lambdas, header-only parts of the standard library (<array>, <algorithm>, <utility>, <type_traits>, <limits>, <initializer_list>, <new>, <cstdint>, <cstring>) and a container
   that is all header (<vector> with the library's operator new). */
#include <new>
#include <array>
#include <algorithm>
#include <utility>
#include <vector>
#include <type_traits>
#include <limits>
#include <initializer_list>
#include <cstdint>
#include <cstring>
#include <stdio.h>

extern "C" void __modlib_cxx_init (void);
extern "C" void __modlib_cxx_fini (void);
void second_unit (void);
extern int second_count;

struct Tracer
{
  const char *name;
  Tracer (const char *n) : name (n) { printf ("ctor %s\n", name); }
  ~Tracer () { printf ("dtor %s\n", name); }
};
static Tracer t1 ("t1"), t2 ("t2");
Tracer t3 ("t3");

struct Shape
{
  virtual ~Shape () { printf ("~Shape\n"); }
  virtual int area () const = 0;
  virtual const char *name () const { return "shape"; }
};
struct Rect : Shape
{
  int w, h;
  Rect (int a, int b) : w (a), h (b) {}
  ~Rect () override { printf ("~Rect\n"); }
  int area () const override { return w * h; }
  const char *name () const override { return "rect"; }
};
struct Square : Rect
{
  Square (int s) : Rect (s, s) {}
  const char *name () const override { return "square"; }
};
struct A { int a = 1; virtual int fa () { return a; } };
struct B { int b = 2; virtual int fb () { return b; } };
struct AB : A, B { int fa () override { return a + 10; } int fb () override { return b + 20; } };

template <class T, int N> struct Stack
{
  T items[N];
  int top = 0;
  bool push (T v) { if (top == N) return false; items[top++] = v; return true; }
  T pop () { return items[--top]; }
};
template <class T> T maxof (T a, T b) { return a > b ? a : b; }
template <class T> struct Sum { static T of (std::initializer_list<T> l) { T s = T (); for (T v : l) s += v; return s; } };

int global_counter = 100;
struct Init { Init () { global_counter += 5; } } init_obj;
static int counter_from_function () { static int calls = 0; return ++calls; }

int main ()
{
  __modlib_cxx_init ();
  printf ("main starts, global_counter %d, second_count %d\n", global_counter, second_count);
  {
    Shape *s[3] = { new Rect (3, 4), new Square (5), new Rect (2, 2) };
    int total = 0;
    for (Shape *p : s) { printf ("%s %d\n", p->name (), p->area ()); total += p->area (); }
    printf ("total %d\n", total);
    for (Shape *p : s) delete p;
  }
  AB ab;
  A *pa = &ab; B *pb = &ab;
  printf ("AB %d %d %d %d\n", pa->fa (), pb->fb (), (int) ((char *) pb > (char *) pa), (int) (sizeof (AB) > 2 * sizeof (int)));          /* (the sizes differ between the host and the ARM: only the facts are printed) */
  Stack<int, 4> st;
  for (int i = 0; i < 6; i++) printf ("push %d: %d\n", i, st.push (i * i));
  { int p1 = st.pop (); int p2 = st.pop (); printf ("pop %d %d\n", p1, p2); }
  printf ("maxof %d %g %c\n", maxof (3, 9), maxof (2.5, 1.5), maxof ('a', 'z'));
  printf ("sum %d %g\n", Sum<int>::of ({ 1, 2, 3, 4 }), Sum<double>::of ({ 0.5, 0.25 }));
  int *arr = new int[5];
  for (int i = 0; i < 5; i++) arr[i] = i * 3;
  printf ("arr %d %d\n", arr[2], arr[4]);
  delete[] arr;
  alignas (8) unsigned char buf[sizeof (Rect)];
  Rect *pr = new (buf) Rect (6, 7);
  printf ("placement %d\n", pr->area ());
  pr->~Rect ();
  { int c1 = counter_from_function (); int c2 = counter_from_function (); int c3 = counter_from_function (); printf ("local statics %d %d %d\n", c1, c2, c3); }
  auto sq = [] (int x) { return x * x; };
  int k = 10;
  auto addk = [k] (int x) { return x + k; };
  printf ("lambdas %d %d\n", sq (7), addk (5));
  std::array<int, 6> ar = { 5, 2, 9, 1, 7, 3 };
  std::sort (ar.begin (), ar.end ());
  printf ("sorted");
  for (int v : ar) printf (" %d", v);
  printf ("\n");
  std::vector<int> vec;
  for (int i = 0; i < 20; i++) vec.push_back (i * 2);
  std::reverse (vec.begin (), vec.end ());
  printf ("vector %d %d %d size %d\n", vec[0], vec[10], vec[19], (int) vec.size ());
  static_assert (std::is_same<decltype (global_counter), int>::value, "int");
  std::pair<int, const char *> pr2 (3, "three");
  std::swap (pr2.first, global_counter);
  printf ("pair %d %s %d limits %d %u\n", pr2.first, pr2.second, global_counter, std::numeric_limits<int>::max (), (unsigned) std::numeric_limits<std::uint8_t>::max ());
  second_unit ();
  printf ("main ends\n");
  __modlib_cxx_fini ();
  return 0;
}
