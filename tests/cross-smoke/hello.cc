#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

int main()
{
  std::vector<std::string> v{"hello", "from", "C++"};
  try { throw std::runtime_error("an exception"); }
  catch (const std::exception &e) { v.push_back(e.what()); }
  std::thread t([&] { v.push_back("thread"); });
  t.join();
  for (const auto &s : v) std::cout << s << ' ';
  std::cout << '\n';
  return 0;
}
