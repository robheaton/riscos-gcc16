/* cxxstd.cc - the parts of the C++ standard library that a module can use (see MODULES.md): built for the host (g++, glibc, libstdc++) and for a module (g++ -mmodule, the headers of libstdc++ and the members
   of libstdc++.a that libstdcxx-mod.a keeps, no exceptions) and run on the A32 interpreter: the two must print the same text.  std::string, std::vector, std::map, std::set, std::list, std::deque,
   std::unique_ptr and std::shared_ptr, std::function and std::bind, <algorithm>, <numeric>, <tuple>, <optional>, <variant>, <string_view>, <bitset>, <array>, <cmath>, <chrono> (the
   types), <atomic>, <mutex> is not there.  Every container is checked after the operations by walking it. */
#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <deque>
#include <memory>
#include <functional>
#include <algorithm>
#include <numeric>
#include <tuple>
#include <optional>
#include <variant>
#include <string_view>
#include <bitset>
#include <array>
#include <cmath>
#include <chrono>
#include <atomic>
#include <cstdio>
#include <cstring>

extern "C" void __modlib_cxx_init (void);
extern "C" void __modlib_cxx_fini (void);

struct Tracer
{
  std::string name;
  Tracer (const char *n) : name (n) { printf ("ctor %s\n", name.c_str ()); }
  ~Tracer () { printf ("dtor %s\n", name.c_str ()); }
};
static Tracer t1 ("static string member");
static std::string global_text = "global string made by a static constructor";
static std::vector<int> global_vec { 5, 4, 3, 2, 1 };
static std::map<std::string, int> global_map { { "one", 1 }, { "two", 2 }, { "three", 3 } };

struct Animal { virtual ~Animal () {} virtual std::string sound () const = 0; };
struct Dog : Animal { std::string sound () const override { return "woof"; } };
struct Cat : Animal { std::string sound () const override { return "meow"; } };

static void strings ()
{
  std::string a = "hello", b = "world";
  std::string c = a + ", " + b + "!";
  printf ("c = %s (%u)\n", c.c_str (), (unsigned) c.size ());
  c.insert (5, " there"); c.erase (0, 1); c.replace (0, 1, "J"); c.append (3, '?'); c += "end";
  printf ("c = %s find(o)=%d rfind(o)=%d substr=%s\n", c.c_str (), (int) c.find ('o'), (int) c.rfind ('o'), c.substr (3, 5).c_str ());
  std::string big;
  for (int i = 0; i < 200; i++) big += (char) ('a' + i % 26);
  printf ("big %u cap>=%d %s\n", (unsigned) big.size (), (int) (big.capacity () >= 200), big.substr (190).c_str ());
  std::string num = std::to_string (12345) + std::to_string (-7);
  printf ("num %s stoi %d stol %ld\n", num.c_str (), std::stoi ("  42xyz"), std::stol ("-900000"));
  printf ("compare %d %d %d\n", a.compare (b) < 0, a == "hello", std::string ("abc") < std::string ("abd"));
  std::string_view sv (c);
  printf ("sv %u %c\n", (unsigned) sv.size (), sv.front ());
  std::string moved = std::move (c);
  printf ("moved %s | empty=%d\n", moved.c_str (), (int) c.empty ());
}
static void containers ()
{
  std::vector<std::string> v { "pear", "apple", "fig", "banana", "cherry" };
  std::sort (v.begin (), v.end ());
  std::string joined;
  for (const auto &s : v) joined += s + ";";
  printf ("sorted %s\n", joined.c_str ());
  std::vector<int> n (20);
  std::iota (n.begin (), n.end (), 1);
  n.erase (std::remove_if (n.begin (), n.end (), [] (int x) { return x % 3 == 0; }), n.end ());
  printf ("n size %u sum %d\n", (unsigned) n.size (), std::accumulate (n.begin (), n.end (), 0));
  std::map<std::string, int> m;
  for (const auto &s : v) m[s] = (int) s.size ();
  m["zzz"] = 99; m.erase ("fig");
  std::string out;
  for (const auto &kv : m) out += kv.first + "=" + std::to_string (kv.second) + " ";
  printf ("map %s (%u) find(apple)=%d find(fig)=%d\n", out.c_str (), (unsigned) m.size (), (int) (m.find ("apple") != m.end ()), (int) (m.find ("fig") != m.end ()));
  std::map<int, std::string> rev;
  for (int i = 20; i > 0; i -= 3) rev[i] = std::string (i % 4 + 1, 'x');
  out.clear ();
  for (auto it = rev.rbegin (); it != rev.rend (); ++it) out += std::to_string (it->first) + it->second + " ";
  printf ("rev %s lower_bound(10)=%d\n", out.c_str (), rev.lower_bound (10)->first);
  std::set<int> s { 9, 3, 7, 1, 3, 9, 5 };
  s.insert (4); s.erase (7);
  out.clear ();
  for (int x : s) out += std::to_string (x) + ",";
  printf ("set %s count(3)=%u count(7)=%u\n", out.c_str (), (unsigned) s.count (3), (unsigned) s.count (7));
  std::list<int> l { 5, 1, 4 };
  l.push_front (9); l.push_back (2); l.sort (); l.reverse (); l.remove (4); l.unique ();
  out.clear ();
  for (int x : l) out += std::to_string (x) + ",";
  printf ("list %s size %u\n", out.c_str (), (unsigned) l.size ());
  std::deque<int> d;
  for (int i = 0; i < 10; i++) { if (i & 1) d.push_front (i); else d.push_back (i); }
  out.clear ();
  for (int x : d) out += std::to_string (x) + ",";
  printf ("deque %s front %d back %d\n", out.c_str (), d.front (), d.back ());
  std::multimap<int, char> mm { { 1, 'a' }, { 2, 'b' }, { 1, 'c' } };
  printf ("multimap count(1)=%u\n", (unsigned) mm.count (1));
  std::unordered_map<std::string, int> um;                                    // (the order of the elements is not the same on the host: only sums and lookups)
  for (int i = 0; i < 100; i++) um[std::to_string (i * 7)] += i;
  int tot = 0;
  for (const auto &kv : um) tot += kv.second + (int) kv.first.size ();
  um.erase ("21");
  printf ("unordered_map %u entries sum %d count(14)=%u count(15)=%u count(21)=%u\n", (unsigned) um.size (), tot, (unsigned) um.count ("14"), (unsigned) um.count ("15"), (unsigned) um.count ("21"));
  std::unordered_set<int> us;
  for (int i = 0; i < 1000; i++) us.insert (i * i % 97);
  int usum = 0;
  for (int x : us) usum += x;
  printf ("unordered_set %u distinct sum %d has(5)=%u has(96)=%u\n", (unsigned) us.size (), usum, (unsigned) us.count (5), (unsigned) us.count (96));
}
static void pointers ()
{
  std::unique_ptr<Animal> p (new Dog);
  std::vector<std::unique_ptr<Animal>> zoo;
  zoo.push_back (std::move (p)); zoo.emplace_back (new Cat); zoo.emplace_back (new Dog);
  std::string s;
  for (const auto &a : zoo) s += a->sound () + " ";
  printf ("zoo %s null=%d\n", s.c_str (), (int) (p == nullptr));
  std::shared_ptr<std::string> sp (new std::string ("shared"));
  std::shared_ptr<std::string> sq = sp;
  printf ("shared %s use_count %ld\n", sq->c_str (), (long) sp.use_count ());
  sq.reset ();
  printf ("after reset %ld\n", (long) sp.use_count ());
  auto ms = std::make_shared<std::string> ("made");
  auto ms2 = ms;
  printf ("make_shared %s use_count %ld\n", ms->c_str (), (long) ms.use_count ());
  std::weak_ptr<std::string> w = sp;
  printf ("weak %d\n", (int) !w.expired ());
}
static int twice (int x) { return 2 * x; }
static void functional ()
{
  std::function<int (int)> f = twice;
  int k = 10;
  std::function<int (int)> g = [k] (int x) { return x + k; };
  auto h = std::bind (twice, 21);
  printf ("function %d %d %d\n", f (4), g (4), h ());
  std::vector<std::function<int ()>> fs;
  for (int i = 0; i < 4; i++) fs.push_back ([i] { return i * i; });
  int t = 0;
  for (auto &x : fs) t += x ();
  printf ("fs %d\n", t);
  auto tup = std::make_tuple (1, std::string ("two"), 3.5);
  printf ("tuple %d %s %g\n", std::get<0> (tup), std::get<1> (tup).c_str (), std::get<2> (tup));
  std::optional<std::string> o;
  printf ("optional %d ", (int) o.has_value ());
  o = "set"; printf ("%d %s\n", (int) o.has_value (), o->c_str ());
  std::variant<int, std::string> v = std::string ("var");
  printf ("variant %d %s\n", (int) v.index (), std::get<std::string> (v).c_str ());
  v = 5; printf ("variant %d %d\n", (int) v.index (), std::get<int> (v));
}
static void misc ()
{
  std::bitset<16> b (0xA5A5); b.flip (0); b.set (15);
  printf ("bitset %s count %u\n", b.to_string ().c_str (), (unsigned) b.count ());
  std::array<double, 4> a { { 1.5, 2.5, 3.5, 4.5 } };
  double s = 0;
  for (double x : a) s += std::sqrt (x) * std::pow (x, 1.5);
  printf ("cmath %.6f %.6f %.6f %d\n", s, std::floor (-2.5), std::fabs (-0.25f), std::abs (-7));
  using namespace std::chrono;
  auto d = milliseconds (2500) + seconds (3);
  printf ("chrono %lld ms %lld s\n", (long long) d.count (), (long long) duration_cast<seconds> (d).count ());
  std::atomic<int> at (5); at += 3; at.fetch_sub (1);
  printf ("atomic %d\n", at.load ());
  char buf[40];
  snprintf (buf, sizeof buf, "%s|%5.2f|%d", global_text.substr (0, 6).c_str (), 3.14159, (int) global_map["two"]);
  printf ("snprintf %s vec %d map %u\n", buf, global_vec[2], (unsigned) global_map.size ());
}

#ifndef CXXSTD_AS_MODULE
int main ()
{
  __modlib_cxx_init ();                                                        /* (the module's own veneers do this when the program is a module: hwpack/cxxstd-module.cc) */
#else
int cxxstd_main ()
{
#endif
  strings ();
  containers ();
  pointers ();
  functional ();
  misc ();
#ifndef CXXSTD_AS_MODULE
  __modlib_cxx_fini ();
#endif
  return 0;
}
