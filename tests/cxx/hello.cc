#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <stdexcept>
int main() {
    std::vector<int> v = {5, 3, 9, 1, 7};
    std::sort(v.begin(), v.end());
    std::map<std::string, int> m = {{"one", 1}, {"two", 2}};
    try { throw std::runtime_error("caught"); } catch (const std::exception &e) { std::cout << e.what() << "\n"; }
    std::cout << "hello from C++ on GCC 16.2: " << v.front() << ".." << v.back() << " " << m["two"] << "\n";
    return 0;
}
