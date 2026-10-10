/* modcxx.cc - a C++ module: cross-smoke.sh compiles it with the flags of module.mk (include-cxx first, then the headers of libstdc++), links it with the C driver and looks at what it was linked with: no
   libstdc++ member that needs the shared library model (a global offset table), no unwinder, and the library members of libstdcxx-mod.a for std::map. */
#include <string>
#include <map>
#include <unordered_map>
#include <vector>
#include <memory>
#include <cstdio>
#include <kernel.h>

struct Shape { virtual ~Shape () {} virtual int area () const = 0; };
struct Sq : Shape { int s; explicit Sq (int v) : s (v) {} int area () const override { return s * s; } };
static std::string greeting = "hello from a static std::string";
static std::map<std::string, int> table { { "a", 1 }, { "b", 2 } };

extern "C" _kernel_oserror *modcxx_init (const char *tail, int podule_base, void *pw)
{
  (void) tail; (void) podule_base; (void) pw;
  std::unordered_map<std::string, int> um;
  std::vector<std::unique_ptr<Shape>> v;
  for (int i = 1; i < 5; i++) { v.emplace_back (new Sq (i)); um[std::to_string (i)] += v.back ()->area (); }
  table["c"] = (int) um.size () + (int) greeting.size ();
  std::printf ("%s %d\n", greeting.c_str (), table["c"] + table["b"]);
  return 0;
}
extern "C" _kernel_oserror *modcxx_final (int fatal, int podule_base, void *pw) { (void) fatal; (void) podule_base; (void) pw; return 0; }
