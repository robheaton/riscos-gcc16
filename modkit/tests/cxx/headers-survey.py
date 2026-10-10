#!/usr/bin/env python3
"""headers-survey.py -- which parts of the C++ standard library a module can use.  Each snippet is a small C++ source that includes headers and uses what they offer; it is compiled the way module.mk
compiles a C++ file (g++ -mmodule -fno-exceptions -fno-rtti -fno-threadsafe-statics, the headers of libstdc++ before the kit's), linked as a module with the C driver (the library of the kit only: there is no
libstdc++) and the result is "ok" (links, no undefined symbol), "undefined: <symbols>" (the header needs code of libstdc++ that a module does not have) or "error" (does not compile).  The expectations
below are what MODULES.md says; the script fails when one is wrong, in either direction.

usage: headers-survey.py [-v] [TC]       (TC: the tool chain with the kit installed; default the work area's tc-cxx)
Exit status 0 = every result as expected."""
import concurrent.futures, os, re, shlex, shutil, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
KIT = os.path.dirname(os.path.dirname(HERE))
args = [a for a in sys.argv[1:] if not a.startswith("-")]
verbose = "-v" in sys.argv
TC = args[0] if args else os.path.expanduser("~/gccsdk-next/tc-cxx")
BIN = os.path.join(TC, "bin")
CMHG = """title-string:           Snip
help-string:            Snip 0.01
date-string:            10 Oct 2026
module-is-c-plus-plus:
initialisation-code:    snip_init
"""
WRAP = """#include <kernel.h>
%(inc)s
volatile int sink;
%(pre)s
int snip (int x)
{
%(body)s
}
extern "C" _kernel_oserror *snip_init (const char *tail, int podule_base, void *pw)
{
  (void) tail; (void) podule_base; (void) pw;
  sink = snip ((int) tail[0]);
  return 0;
}
"""
# name: (includes, code before the function, body, expected, flags).  expected: "ok", "undefined" (links with undefined symbols), "error" (does not compile)
S = {}
def snip(name, inc, body, expect="ok", pre="", flags=""): S[name] = (inc, pre, body, expect, flags)

# ---- headers that need no library code
snip("new", "#include <new>", "char buf[16]; int *p = new (buf) int (x); int *q = new (std::nothrow) int (x); sink = (int) (long) q; int r = *p; delete q; return r;")
snip("array", "#include <array>", "std::array<int, 4> a = {{x, 1, 2, 3}}; return (int) a.size () + a[(unsigned) x & 3] + a.at (2);")
snip("algorithm", "#include <algorithm>", "int a[8] = {5, 3, 8, 1, 9, 2, 7, x}; std::sort (a, a + 8); std::reverse (a, a + 8); int *p = std::find (a, a + 8, 7); std::stable_sort (a, a + 8); return *p + std::min (a[0], a[1]) + std::max (a[2], a[3]) + (int) (std::lower_bound (a, a + 8, 4) - a);")
snip("vector", "#include <vector>", "std::vector<int> v; for (int i = 0; i < x; i++) v.push_back (i * 3); v.resize (20); v.insert (v.begin () + 2, 99); v.erase (v.begin ()); int t = 0; for (int e : v) t += e; return t + (int) v.size ();")
snip("utility", "#include <utility>", "std::pair<int, int> p (x, 2); int a = 1, b = 2; std::swap (a, b); int c = std::exchange (a, 7); return p.first + p.second + a + b + c + std::move (x);")
snip("type_traits", "#include <type_traits>", "static_assert (std::is_same<int, int>::value && !std::is_same<int, long>::value && std::is_integral<int>::value, \"traits\"); return x + (int) std::is_trivially_copyable<int>::value;")
snip("limits", "#include <limits>", "return std::numeric_limits<int>::max () / x + (int) (std::numeric_limits<double>::epsilon () * 1e20) + (int) std::numeric_limits<unsigned char>::digits;")
snip("initializer_list", "#include <initializer_list>", "return sum ({x, 2, 3});", pre="static int sum (std::initializer_list<int> l) { int t = 0; for (int e : l) t += e; return t; }")
snip("numeric", "#include <numeric>\n#include <array>", "std::array<int, 5> a = {{1, 2, 3, 4, x}}; int t = std::accumulate (a.begin (), a.end (), 0); std::iota (a.begin (), a.end (), x); return t + a[4] + (int) std::gcd (12, x) + std::lcm (4, 6);")
snip("iterator", "#include <iterator>\n#include <array>\n#include <algorithm>", "std::array<int, 4> a = {{4, 3, 2, x}}; std::array<int, 4> b; std::reverse_copy (a.begin (), a.end (), std::begin (b)); return (int) std::distance (a.begin (), a.end ()) + b[0] + *std::next (b.begin ());")
snip("tuple", "#include <tuple>", "std::tuple<int, char, int> t = std::make_tuple (x, 'a', 3); int a, c; char b; std::tie (a, b, c) = t; return std::get<0> (t) + std::get<2> (t) + a + b + c;")
snip("functional", "#include <functional>", "std::function<int (int)> f = [x] (int y) { return x + y; }; auto g = std::bind (f, 5); return f (1) + g () + std::plus<int> () (x, 1) + (int) std::less<int> () (x, 3);")
snip("memory-unique_ptr", "#include <memory>", "std::unique_ptr<int> p (new int (x)); auto q = std::make_unique<int[]> (4); q[1] = *p; std::unique_ptr<int[]> r = std::move (q); return r[1] + *p;")
snip("memory-shared_ptr", "#include <memory>", "std::shared_ptr<int> p (new int (x)); std::shared_ptr<int> q = p; return *q + (int) p.use_count ();")
snip("memory-make_shared", "#include <memory>", "std::shared_ptr<int> p = std::make_shared<int> (x); return *p;")
snip("optional", "#include <optional>", "std::optional<int> o; if (x > 1) o = x; return o.value_or (7) + (o ? *o : 0);", flags="-std=gnu++17")
snip("variant", "#include <variant>", "std::variant<int, double> v = x; if (x > 100) v = 2.5; return std::holds_alternative<int> (v) ? std::get<int> (v) : (int) std::get<double> (v);", flags="-std=gnu++17")
snip("string_view", "#include <string_view>", "std::string_view s (\"hello world\"); return (int) s.size () + (int) s.find ('w') + (int) s.substr (6).size () + x;", flags="-std=gnu++17")
snip("bitset", "#include <bitset>", "std::bitset<32> b (x); b.set (5); return (int) b.count () + (int) b.size ();")
snip("random", "#include <random>", "std::mt19937 g (x); std::uniform_int_distribution<int> d (1, 6); return d (g) + (int) (g () & 3);", expect="error")   # needs std::lgamma (poisson, binomial): not in the kit yet
snip("chrono", "#include <chrono>", "std::chrono::milliseconds ms (x); std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds> (ms * 5000); return (int) s.count ();")
snip("atomic", "#include <atomic>", "std::atomic<int> a (x); a.fetch_add (2); int e = 5; a.compare_exchange_strong (e, 9); return a.load () + (int) a.exchange (3);")
snip("span", "#include <span>\n#include <array>", "std::array<int, 4> a = {{1, 2, 3, x}}; std::span<int> s (a); return (int) s.size () + s[3] + (int) s.first (2).size ();", flags="-std=gnu++20")
snip("cmath", "#include <cmath>", "return (int) (std::sqrt ((double) x) + std::pow (2.0, x) + std::sin (1.0f * (float) x) + std::floor (2.5));")
snip("cstring-cstdio-cstdlib", "#include <cstring>\n#include <cstdio>\n#include <cstdlib>", "char b[32]; std::snprintf (b, sizeof b, \"%d\", x); std::memcpy (b + 8, b, 4); return (int) std::strlen (b) + std::atoi (b) + std::abs (-x);")
snip("c-headers", "#include <cstdint>\n#include <cstddef>\n#include <climits>\n#include <cctype>\n#include <cerrno>\n#include <cassert>\n#include <csetjmp>\n#include <csignal>\n#include <cstdarg>\n#include <ctime>", "std::int64_t v = x; std::size_t n = sizeof v; return (int) (v + (long long) n) + std::isdigit (x) + INT_MAX / 7 + errno + (int) std::time (0);")
snip("math.h-stdlib.h", "#include <math.h>\n#include <stdlib.h>\n#include <stdio.h>\n#include <string.h>\n#include <ctype.h>", "return (int) sqrt ((double) x) + abs (x) + (int) strlen (\"abc\") + toupper ('a') + printf (\"%d\", x);")

# ---- language features
snip("lang-virtual-mi", "", "struct A { virtual int f () { return 1; } virtual ~A () {} }; struct B { virtual int g () { return 2; } virtual ~B () {} }; struct C : A, B { int f () override { return 10; } int g () override { return 20; } }; C c; A *a = &c; B *b = &c; return a->f () + b->g () + x;")
snip("lang-new-array-dtor", "", "struct T { int v; T () : v (3) {} ~T () { sink = sink + 1; } }; T *p = new T[4]; int r = p[2].v; delete[] p; return r + x;")
snip("lang-local-static-dtor", "", "static struct D { D () { sink = 1; } ~D () { sink = 2; } } d; (void) d; return x;")
snip("lang-global-operator-new", "#include <new>\n#include <cstdlib>", "int *p = new int (x); int r = *p; delete p; return r;", pre="void *operator new (std::size_t n) { sink = (int) n; return std::malloc (n); }\nvoid operator delete (void *p) noexcept { std::free (p); }\nvoid operator delete (void *p, std::size_t) noexcept { std::free (p); }")
snip("lang-constexpr-lambda-structured", "#include <utility>", "constexpr int k = 3 * 4; auto f = [=] (int y) { return y + k; }; std::pair<int, int> p (x, 2); auto [a, b] = p; if constexpr (sizeof (int) == 4) return f (a) + b; else return 0;", flags="-std=gnu++17")
snip("lang-pure-virtual", "", "struct P { virtual int f () = 0; virtual ~P () {} }; struct Q : P { int f () override { return 4; } }; Q q; P *p = &q; return p->f () + x;")
snip("lang-template-recursion", "", "return fact<5> () + x;", pre="template <int N> constexpr int fact () { return N * fact<N - 1> (); } template <> constexpr int fact<0> () { return 1; }")

# ---- not available: they need the compiled library (or more of the C library than the kit has)
snip("string", "#include <string>", "std::string s (\"abc\"); s += \"def\"; s.append (3, 'x'); return (int) s.size () + (int) s.find ('d') + x;")
snip("map", "#include <map>", "std::map<int, int> m; m[x] = 3; m[1] = 2; return (int) m.size () + m[x];")
snip("set", "#include <set>", "std::set<int> s; s.insert (x); s.insert (3); return (int) s.size ();")
snip("list", "#include <list>", "std::list<int> l; l.push_back (x); l.push_back (3); l.push_front (1); int t = 0; for (int e : l) t += e; return t;")
snip("deque", "#include <deque>", "std::deque<int> d; d.push_back (x); d.push_front (3); return d.front () + (int) d.size ();")
snip("unordered_map", "#include <unordered_map>", "std::unordered_map<int, int> m; m[x] = 3; return m[x] + (int) m.size ();")
snip("iostream", "#include <iostream>", "std::cout << x; return 0;", expect="undefined")
snip("sstream", "#include <sstream>", "std::ostringstream o; o << x; return (int) o.str ().size ();", expect="undefined")
snip("stdexcept", "#include <stdexcept>", "std::runtime_error e (\"x\"); return e.what ()[0] + x;", expect="undefined")
snip("mutex", "#include <mutex>", "std::mutex m; m.lock (); m.unlock (); return x;", expect="error")
snip("thread", "#include <thread>", "std::thread t ([] {}); t.join (); return x;", expect="error")
snip("complex", "#include <complex>", "std::complex<double> c (x, 2); c = c * c + std::exp (c); return (int) std::abs (c);")
snip("lang-throw", "", "if (x > 100) throw 5; return x;", expect="error")
snip("lang-try-catch", "", "try { return x; } catch (...) { return 0; }", expect="error")
snip("lang-dynamic-cast", "", "struct A { virtual ~A () {} }; struct B : A {}; B b; A *a = &b; return dynamic_cast<B *> (a) != 0;", expect="error")
snip("lang-typeid", "#include <typeinfo>", "return typeid (x).name ()[0];", expect="error")
snip("lang-thread-local", "", "static thread_local int t; return t + x;", expect="undefined")


def flags_of(w):
    """MODCXXFLAGS of module.mk, asked of make itself"""
    open(os.path.join(w, "cxxmod.cmhg"), "w").write(CMHG)
    open(os.path.join(w, "mk"), "w").write("MODULE = Snip\nCMHG = cxxmod.cmhg\nSRCS = snip.cc\ninclude %s/module.mk\nprint-flags:\n\t@echo $(MODCXXFLAGS)\n" % KIT)
    r = subprocess.run(["make", "-s", "-C", w, "-f", "mk", "BIN=" + BIN, "print-flags"], capture_output=True, text=True)
    if r.returncode: sys.exit("make print-flags failed:\n" + r.stderr)
    return shlex.split(r.stdout.strip().split("\n")[-1])


def run_one(w, flags, name):
    inc, pre, body, expect, extra = S[name]
    d = os.path.join(w, re.sub(r"\W", "_", name)); os.makedirs(d)
    src = os.path.join(d, "snip.cc")
    open(src, "w").write(WRAP % dict(inc=inc, pre=pre, body="  " + body))
    fl = [f for f in flags if not f.startswith("-std=")] + ([extra] if extra else ["-std=gnu++17"])
    fl = [f if not f.startswith("-I" + w) else f for f in fl]
    cc = os.path.join(BIN, "arm-riscos-gnueabihf-g++")
    r = subprocess.run([cc] + fl + ["-I" + w, "-x", "c++", "-c", src, "-o", os.path.join(d, "snip.o")], capture_output=True, text=True)
    if r.returncode: return name, "error", r.stderr
    r = subprocess.run([os.path.join(BIN, "arm-riscos-gnueabihf-gcc"), "-mmodule", "-o", os.path.join(d, "snip.elf"), os.path.join(d, "snip.o"), os.path.join(w, "header.o")], capture_output=True, text=True)
    if r.returncode:
        und = sorted(set(re.findall(r"undefined reference to [`']([^'`]+)'", r.stderr)))
        return name, "undefined", ", ".join(und) if und else r.stderr
    return name, "ok", ""


w = tempfile.mkdtemp(prefix="cxxsurvey-")
try:
    flags = flags_of(w)
    # header.h and header.o from the CMHG file
    env = dict(os.environ, CMUNGE_CC=os.path.join(BIN, "arm-riscos-gnueabihf-gcc"))
    subprocess.run([os.path.join(BIN, "cmunge"), "-tgcc", "-32bit", "-p", "-d", os.path.join(w, "header.h"), "-s", os.path.join(w, "header.s"), os.path.join(w, "cxxmod.cmhg")], check=True, env=env)
    subprocess.run([os.path.join(BIN, "arm-riscos-gnueabihf-gcc"), "-mmodule", "-c", os.path.join(w, "header.s"), "-o", os.path.join(w, "header.o")], check=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=12) as ex:
        results = list(ex.map(lambda n: run_one(w, flags, n), S))
    bad = 0
    print("%-36s %-10s %s" % ("snippet", "result", "(undefined symbols / first error)"))
    for name, res, detail in results:
        want = S[name][3]
        mark = "" if res == want else "   <-- EXPECTED " + want
        if res != want: bad += 1
        first = detail.strip().split("\n")[0][:150] if res == "error" else detail[:150]
        print("%-36s %-10s %s%s" % (name, res, first if res != "ok" else "", mark))
        if verbose and res == "error": print(detail[:1500])
    print()
    print("%d snippets, %d as expected, %d not" % (len(results), len(results) - bad, bad))
    sys.exit(1 if bad else 0)
finally:
    shutil.rmtree(w, ignore_errors=True)
