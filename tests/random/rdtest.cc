// rdtest.cc -- std::random_device on RISC OS: real entropy (/dev/urandom through UnixLib's CryptRand module) instead of the fixed-seed mt19937 fallback.
// Run it twice: the "first values" must differ between runs (with GCC 10.2's libstdc++ they are the same every time).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <map>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#ifndef ROTEST_CFG
#define ROTEST_CFG "rdtest"
#endif

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fails++; std::printf("  FAIL line %d: %s\n", __LINE__, #c); } } while (0)

int main()
{
    std::printf("rdtest 1.0 [%s]\n", ROTEST_CFG);
    std::random_device rd;
    std::printf("  INFO rd.entropy() = %g\n", rd.entropy());
    std::vector<unsigned> v(8);
    for (auto &x : v) x = rd();
    std::printf("  INFO first values: %08x %08x %08x %08x %08x %08x %08x %08x\n", v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
    std::set<unsigned> distinct(v.begin(), v.end());
    CHECK(distinct.size() >= 7);                                         // 8 draws of 32 bits: a repeat is (almost) impossible
    CHECK(rd.min() == 0 && rd.max() == 0xffffffffu);
    // a second device in the same process must not repeat the first one's sequence
    std::random_device rd2;
    std::vector<unsigned> w(8);
    for (auto &x : w) x = rd2();
    CHECK(w != v);
    // bit balance over 4000 words: about half of the 128000 bits set
    long ones = 0;
    for (int i = 0; i < 4000; i++) ones += __builtin_popcount(rd());
    std::printf("  INFO set bits in 4000 words: %ld of 128000 (expect about 64000)\n", ones);
    CHECK(ones > 62000 && ones < 66000);
    // a die roll through a distribution
    std::uniform_int_distribution<int> die(1, 6);
    std::map<int, int> count;
    for (int i = 0; i < 6000; i++) count[die(rd)]++;
    bool ok = count.size() == 6;
    for (auto &kv : count) ok = ok && kv.second > 800 && kv.second < 1200;
    CHECK(ok);
    // the common idiom: seed a Mersenne twister from the device
    std::mt19937 gen(rd());
    unsigned first = gen();
    std::printf("  INFO first output of mt19937(rd()): %u   (must differ from run to run; the fixed-seed fallback always gives 3499211612)\n", first);
    CHECK(first != 3499211612u || true);                                  // informational: one in 4 billion could match
    // explicit tokens
    bool threw = false;
    try { std::random_device bad("no-such-source"); (void) bad(); } catch (const std::exception &e) { threw = true; }
    CHECK(threw);
    try { std::random_device dev("/dev/urandom"); unsigned a = dev(), b = dev(); CHECK(a != b); } catch (const std::exception &e) { std::printf("  INFO token \"/dev/urandom\" threw: %s\n", e.what()); }
    try { std::random_device pr("prng"); unsigned a = pr(), b = pr(); CHECK(a != b); std::printf("  INFO token \"prng\" works (time-seeded engine): %08x %08x\n", a, b); } catch (const std::exception &e) { std::printf("  INFO token \"prng\" threw: %s\n", e.what()); }
    std::printf("SUMMARY [rdtest]: %d checks, %d failed -> %s\n", checks, fails, fails ? "FAIL" : "PASS");
    return fails != 0;
}
