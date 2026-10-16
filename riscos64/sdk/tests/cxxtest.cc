// C++ with libstdc++ on the SDK: containers, strings, iostream, exceptions, static constructors and destructors, atexit.  Built with aarch64-riscos-g++.
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <cstdlib>
struct Static { Static () { std::cout << "static ctor\n"; } ~Static () { std::cout << "static dtor\n"; } } the_static;
static void at_exit () { std::cout << "atexit handler\n"; }
int main (int argc, char **argv)
{
  std::vector<std::string> v = { "pear", "apple", "fig", "cherry" };
  std::sort (v.begin (), v.end ());
  std::map<std::string, int> m;
  for (auto &s : v) m[s] = (int) s.size ();
  auto p = std::make_unique<int> (42);
  std::atexit (at_exit);
  std::cout << "hello from C++ on RISC OS 64, argc = " << argc << ", *p = " << *p << "\n";
  for (auto &kv : m) std::cout << kv.first << " " << kv.second << "\n";
  try { throw std::runtime_error ("an exception"); } catch (const std::exception &e) { std::cout << "caught: " << e.what () << "\n"; }
  try { std::vector<int> e; e.at (3); } catch (const std::out_of_range &) { std::cout << "caught out_of_range\n"; }
  return 0;
}
