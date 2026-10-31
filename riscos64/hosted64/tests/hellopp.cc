#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>
int main ()
{
  std::vector<std::string> v = { "pear", "apple", "fig" };
  std::sort (v.begin (), v.end ());
  for (auto &s : v) std::cout << s << "\n";
  try { throw std::runtime_error ("boom"); } catch (const std::exception &e) { std::cout << "caught " << e.what () << "\n"; }
  std::cout << "C++ for 32 bit RISC OS, compiled on RISC OS 64\n";
  return 0;
}
