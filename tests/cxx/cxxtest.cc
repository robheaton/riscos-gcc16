// cxxtest.cc -- RISC OS C++ regression checks (self-checking; see rotest.c for the C equivalent).
// Prints one line per group, FAIL lines for problems and "SUMMARY ... PASS|FAIL".  INFO lines are
// properties of the runtime that are reported but do not fail the run.
// Build as C++17 (works with GCC 10.2.0 and 16.2.0).  Opt-in extras: run with "threads" for the
// pthread tests.
#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <limits>
#include <list>
#include <locale>
#include <map>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <new>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#ifndef ROTEST_CFG
#define ROTEST_CFG "unknown"
#endif

static int g_checks, g_fail, g_gf, g_info, g_info_fail, g_ip;
static const char *g_grp = "?";

static void failv(int line, const char *fmt, ...)
{
    va_list ap;
    g_fail++;
    if (g_gf++ < 6) {
        std::printf("FAIL %s:%d ", g_grp, line);
        va_start(ap, fmt);
        std::vprintf(fmt, ap);
        va_end(ap);
        std::printf("\n");
    } else if (g_gf == 7) {
        std::printf("  ... more failures in this group suppressed\n");
    }
}
#define CHECKV(cond, ...) do { g_checks++; if (!(cond)) failv(__LINE__, __VA_ARGS__); } while (0)
#define CHECK(cond) CHECKV(cond, "%s", #cond)
#define INFOV(cond, ...) do { g_info++; if (!(cond)) { g_info_fail++; if (g_ip++ < 8) { std::printf("INFO %s:%d ", g_grp, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } } while (0)

// ------------------------------------------------------------------ globals: static init / destruction
static int g_ctor_before_main = 0;
struct GlobalInit { GlobalInit() { g_ctor_before_main = 17; } };
static GlobalInit g_global_init;
static int g_dtors;
struct Raii { Raii() = default; ~Raii() { ++g_dtors; } };

// ------------------------------------------------------------------ exceptions
struct Base { virtual ~Base() = default; virtual int f() const { return 1; } int tag = 7; };
struct Derived : Base { int f() const override { return 2; } };
struct MyErr : std::runtime_error { int code; MyErr(const char *m, int c) : std::runtime_error(m), code(c) {} };
struct BigErr { char payload[300]; int n; };
static int g_copies;
struct CopyCounter { CopyCounter() = default; CopyCounter(const CopyCounter &) { ++g_copies; } CopyCounter(CopyCounter &&) noexcept { } };

static void __attribute__((noinline)) thrower(int depth)
{
    Raii r;
    if (depth == 0) throw MyErr("deep", 42);
    thrower(depth - 1);
}
static int __attribute__((noinline)) catch_depth(int depth)
{
    try { thrower(depth); } catch (const MyErr &e) { return e.code + g_dtors; }
    return -1;
}
// throw through frames with alloca, VLAs and over-aligned locals (exercises unwind info with a frame pointer)
static int __attribute__((noinline)) odd_frame(int n)
{
    alignas(32) char a32[40];
    char *p = static_cast<char *>(__builtin_alloca(n + 1));
    int vla[n + 1];
    std::memset(a32, 1, sizeof a32);
    std::memset(p, 2, n + 1);
    for (int i = 0; i <= n; i++) vla[i] = i;
    Raii r;
    if (n % 2) throw std::range_error("odd");
    return a32[3] + p[0] + vla[n];
}
static int __attribute__((noinline)) odd_frame_outer(int n)
{
    volatile int v[8];
    v[0] = 3;
    try { return odd_frame(n) + v[0]; } catch (const std::range_error &) { return -77; }
}
struct ThrowInCtor {
    Raii a;
    ThrowInCtor() { throw std::logic_error("ctor"); }
    Raii b;
};
static void __attribute__((noinline)) throw_big() { BigErr e{}; e.n = 99; throw e; }
static void __attribute__((noinline)) throw_counter() { throw CopyCounter(); }

static void t_exceptions()
{
    g_dtors = 0;
    CHECK(catch_depth(10) == 42 + 11);
    try { throw MyErr("x", 1); } catch (const std::exception &e) { CHECK(std::strcmp(e.what(), "x") == 0); }
    try { try { throw 5; } catch (int) { throw; } } catch (int v) { CHECK(v == 5); }
    try { throw std::string("str"); } catch (...) { CHECK(true); }
    try { throw 1.5; } catch (int) { CHECK(false); } catch (double d) { CHECK(d == 1.5); }
    try { throw_big(); } catch (const BigErr &e) { CHECK(e.n == 99); }
    g_copies = 0;
    try { throw_counter(); } catch (const CopyCounter &) { }
    CHECKV(g_copies == 0, "exception object copied %d times", g_copies);
    g_dtors = 0;
    try { ThrowInCtor t; } catch (const std::logic_error &e) { CHECK(std::strcmp(e.what(), "ctor") == 0); }
    CHECKV(g_dtors == 2, "member destructors during failed construction: %d (both members were already built)", g_dtors);
    for (int n = 0; n < 9; n++) CHECKV(odd_frame_outer(n) == ((n % 2) ? -77 : 1 + 2 + n + 3), "odd_frame(%d)", n);
    // nested exceptions and exception_ptr
    std::exception_ptr ep;
    try { throw MyErr("ep", 5); } catch (...) { ep = std::current_exception(); }
    try { std::rethrow_exception(ep); } catch (const MyErr &e) { CHECK(e.code == 5); }
    try {
        try { throw std::runtime_error("inner"); }
        catch (...) { std::throw_with_nested(std::logic_error("outer")); }
    } catch (const std::logic_error &e) {
        bool nested = false;
        try { std::rethrow_if_nested(e); } catch (const std::runtime_error &in) { nested = std::strcmp(in.what(), "inner") == 0; }
        CHECK(nested);
    }
    // standard library exceptions
    { std::vector<int> v(3); bool c = false; try { (void)v.at(100); } catch (const std::out_of_range &) { c = true; } CHECK(c); }
    { bool c = false; try { (void)std::stoi("abc"); } catch (const std::invalid_argument &) { c = true; } CHECK(c); }
    { bool c = false; try { (void)std::stoi("99999999999999999999"); } catch (const std::out_of_range &) { c = true; } CHECK(c); }
    { bool c = false; try { std::function<int()> f; f(); } catch (const std::bad_function_call &) { c = true; } CHECK(c); }
    { bool c = false; try { std::optional<int> o; (void)o.value(); } catch (const std::bad_optional_access &) { c = true; } CHECK(c); }
    { bool c = false; try { std::variant<int, double> v = 1; (void)std::get<double>(v); } catch (const std::bad_variant_access &) { c = true; } CHECK(c); }
    { bool c = false; try { Base b; (void)dynamic_cast<Derived &>(b); } catch (const std::bad_cast &) { c = true; } CHECK(c); }
#ifndef NO_HUGE_ALLOC
    // an impossible request must throw bad_alloc.  The size is SIZE_MAX, not SIZE_MAX / 2: on a 32-bit RISC OS system the latter is 2 GB - 1, which UnixLib's malloc hands to ARMEABISupport's mmap twice
    // (the large request, then "mmap as MORECORE"); both fail, and each failure leaves an empty 100 MB dynamic area behind until the next reboot (an ARMEABISupport bug: eight runs of this program used
    // up 1.6 GB of address space).  malloc refuses SIZE_MAX itself, before any mmap.
    { bool c = false; volatile std::size_t huge = static_cast<std::size_t>(-1); try { char *p = new char[huge]; *static_cast<volatile char *>(p) = 1; delete[] p; } catch (const std::bad_alloc &) { c = true; } CHECK(c); }
#endif
    { bool c = false; try { std::string s("abc"); (void)s.substr(10); } catch (const std::out_of_range &) { c = true; } CHECK(c); }
    // noexcept contract and nothrow new
    { char *p = new (std::nothrow) char[16]; CHECK(p != nullptr); delete[] p; }
    CHECK(noexcept(std::declval<int &>() = 1));
}

// ------------------------------------------------------------------ RTTI and virtual dispatch
struct A { virtual ~A() = default; virtual int who() const { return 1; } int a = 10; };
struct B : virtual A { int who() const override { return 2; } int b = 20; };
struct C : virtual A { int who() const override { return 3; } int c = 30; };
struct D : B, C { int who() const override { return 4; } int d = 40; };
struct I1 { virtual int i1() const = 0; virtual ~I1() = default; };
struct I2 { virtual int i2() const = 0; virtual ~I2() = default; };
struct Impl : I1, I2 { int i1() const override { return 11; } int i2() const override { return 22; } };

static void t_rtti()
{
    D d;
    A *pa = &d;
    B *pb = &d;
    C *pc = &d;
    CHECK(pa->who() == 4 && pb->who() == 4 && pc->who() == 4);
    CHECK(dynamic_cast<D *>(pa) == &d);
    CHECK(dynamic_cast<B *>(pa) == pb);
    CHECK(dynamic_cast<C *>(pa) == pc);
    CHECK(dynamic_cast<void *>(pa) == static_cast<void *>(&d));
    CHECK(pb->a == 10 && pc->a == 10 && pb->b == 20 && pc->c == 30 && d.d == 40);
    std::unique_ptr<A> plain(new A);
    CHECK(dynamic_cast<B *>(plain.get()) == nullptr);
    CHECK(typeid(*pa) == typeid(D));
    CHECK(typeid(*pa) != typeid(A));
    CHECK(typeid(int) != typeid(unsigned));
    CHECK(std::strcmp(typeid(int).name(), typeid(int).name()) == 0);
    Impl impl;
    I1 *i1 = &impl;
    I2 *i2 = &impl;
    CHECK(i1->i1() == 11 && i2->i2() == 22);
    CHECK(dynamic_cast<I2 *>(i1) == i2);
    std::unique_ptr<A> up(new D);
    CHECK(up->who() == 4);
    std::vector<std::unique_ptr<A>> v;
    v.emplace_back(new A);
    v.emplace_back(new B);
    v.emplace_back(new C);
    v.emplace_back(new D);
    int sum = 0;
    for (auto &p : v) sum += p->who();
    CHECK(sum == 1 + 2 + 3 + 4);
}

// ------------------------------------------------------------------ containers and algorithms
static unsigned lcg(unsigned &s) { s = s * 1103515245u + 12345u; return s >> 8; }

static void t_containers()
{
    unsigned s = 4242;
    std::vector<int> v;
    for (int i = 0; i < 2000; i++) v.push_back(static_cast<int>(lcg(s) % 10000));
    std::vector<int> ref = v;
    std::sort(v.begin(), v.end());
    CHECK(std::is_sorted(v.begin(), v.end()));
    long long total = std::accumulate(ref.begin(), ref.end(), 0LL);
    CHECK(std::accumulate(v.begin(), v.end(), 0LL) == total);
    std::stable_sort(ref.begin(), ref.end(), [](int a, int b) { return a / 100 < b / 100; });
    CHECK(std::is_sorted(ref.begin(), ref.end(), [](int a, int b) { return a / 100 < b / 100; }));
    auto it = std::lower_bound(v.begin(), v.end(), v[777]);
    CHECK(it != v.end() && *it == v[777]);
    std::vector<int> nth(v.rbegin(), v.rend());
    std::nth_element(nth.begin(), nth.begin() + 1000, nth.end());
    CHECK(nth[1000] == v[1000]);
    v.erase(std::unique(v.begin(), v.end()), v.end());
    CHECK(std::adjacent_find(v.begin(), v.end()) == v.end());
    v.insert(v.begin() + 3, 123456);
    CHECK(v[3] == 123456);
    v.shrink_to_fit();

    std::map<std::string, int> m;
    for (int i = 0; i < 1000; i++) m["key" + std::to_string(i)] = i * 3;
    CHECK(m.size() == 1000 && m["key999"] == 2997 && m.find("nokey") == m.end());
    std::unordered_map<int, std::string> um;
    for (int i = 0; i < 1000; i++) um[i] = std::to_string(i * i);
    CHECK(um.size() == 1000 && um[31] == "961" && um.count(1000) == 0);
    std::set<int> se(v.begin(), v.end());
    CHECK(se.size() == v.size());
    std::unordered_set<std::string> us{"a", "b", "c"};
    us.insert("a");
    CHECK(us.size() == 3);
    std::deque<int> dq;
    for (int i = 0; i < 500; i++) { dq.push_back(i); dq.push_front(-i); }
    CHECK(dq.size() == 1000 && dq.front() == -499 && dq.back() == 499);
    std::list<int> l{5, 1, 4, 2, 3};
    l.sort();
    CHECK(l.front() == 1 && l.back() == 5);
    std::list<int> l2{10, 20};
    l.splice(l.begin(), l2);
    CHECK(l.front() == 10 && l.size() == 7);
    std::priority_queue<int> pq;
    for (int x : {3, 9, 1, 7}) pq.push(x);
    CHECK(pq.top() == 9);
    std::array<int, 5> arr{{5, 4, 3, 2, 1}};
    std::sort(arr.begin(), arr.end());
    CHECK(arr[0] == 1 && arr[4] == 5);
    std::vector<double> dv{1.5, 2.5, 3.5};
    std::vector<double> out(3);
    std::transform(dv.begin(), dv.end(), out.begin(), [](double x) { return x * 2; });
    CHECK(out[2] == 7.0);
    std::partial_sum(dv.begin(), dv.end(), out.begin());
    CHECK(out[2] == 7.5);
    CHECK(std::inner_product(dv.begin(), dv.end(), dv.begin(), 0.0) == 1.5 * 1.5 + 2.5 * 2.5 + 3.5 * 3.5);
}

// ------------------------------------------------------------------ strings and streams
static void t_strings()
{
    std::string s = "hello, world";
    CHECK(s.size() == 12 && s.find("world") == 7 && s.rfind('o') == 8);
    s.replace(0, 5, "HELLO");
    CHECK(s == "HELLO, world");
    CHECK(s.substr(7, 3) == "wor" && s.compare("HELLO") > 0);
    std::string r(s.rbegin(), s.rend());
    CHECK(r == "dlrow ,OLLEH");
    std::string_view sv(s);
    CHECK(sv.substr(0, 5) == "HELLO" && sv.find(',') == 5);
    CHECK(std::to_string(-123) == "-123" && std::to_string(4000000000u) == "4000000000");
    CHECK(std::stoi("  42x") == 42 && std::stol("-77") == -77 && std::stod("2.5e2") == 250.0);
    CHECK(std::stoull("18446744073709551615") == 18446744073709551615ull);
    std::ostringstream os;
    os << std::setw(8) << std::setfill('0') << 42 << '|' << std::hex << 255 << '|' << std::dec << std::fixed << std::setprecision(3) << 3.14159
       << '|' << std::scientific << std::setprecision(2) << 12345.678 << '|' << std::boolalpha << true << '|' << std::left << std::setw(5) << std::setfill('.') << "ab";
    CHECKV(os.str() == "00000042|ff|3.142|1.23e+04|true|ab...", "ostringstream gave '%s'", os.str().c_str());
    std::istringstream is("12 3.5 word -7");
    int i1 = 0, i2 = 0; double d = 0; std::string w;
    is >> i1 >> d >> w >> i2;
    CHECK(i1 == 12 && d == 3.5 && w == "word" && i2 == -7);
    std::ostringstream os2;
    os2 << 1.0 / 3.0 << ' ' << 1e20 << ' ' << 100000.0 << ' ' << 0.5f << ' ' << -0.0 << ' ' << std::setprecision(17) << 0.1;
    CHECKV(os2.str() == "0.333333 1e+20 100000 0.5 -0 0.10000000000000001", "double formatting gave '%s'", os2.str().c_str());
    std::ostringstream os3;
    os3 << 123456789012345LL << ' ' << -5LL << ' ' << 255u << ' ' << static_cast<unsigned long long>(18446744073709551615ull);
    CHECK(os3.str() == "123456789012345 -5 255 18446744073709551615");
    // writing to the real stream objects (output is checked by eye / not captured)
    std::cout << "  [iostream ok]" << std::endl;
    std::cerr << "";
    std::string big(100000, 'x');
    big += "end";
    CHECK(big.size() == 100003 && big.back() == 'd');
    std::string cat;
    for (int i = 0; i < 1000; i++) cat += std::to_string(i);
    CHECK(cat.size() == 2890 && cat.substr(0, 10) == "0123456789");
}

// ------------------------------------------------------------------ language features and library utilities
constexpr int cfib(int n) { return n < 2 ? n : cfib(n - 1) + cfib(n - 2); }
constexpr std::array<int, 5> make_sq() { std::array<int, 5> a{}; for (int i = 0; i < 5; i++) a[i] = i * i; return a; }
static_assert(cfib(10) == 55, "constexpr");
static_assert(make_sq()[4] == 16, "constexpr array");

template <typename T> T tsum(const std::vector<T> &v) { T s{}; for (auto &x : v) s += x; return s; }
template <typename... Ts> auto fold_sum(Ts... ts) { return (ts + ...); }
template <typename T> std::string kind() { if constexpr (std::is_integral_v<T>) return "int"; else if constexpr (std::is_floating_point_v<T>) return "fp"; else return "other"; }

struct Visitor {
    std::string operator()(int i) const { return "i" + std::to_string(i); }
    std::string operator()(double d) const { return "d" + std::to_string(static_cast<int>(d)); }
    std::string operator()(const std::string &s) const { return "s" + s; }
};

static void t_language()
{
    CHECK(cfib(20) == 6765);
    CHECK(tsum(std::vector<int>{1, 2, 3}) == 6 && tsum(std::vector<double>{0.5, 0.25}) == 0.75);
    CHECK(fold_sum(1, 2, 3, 4) == 10 && fold_sum(1.5, 2.5) == 4.0);
    CHECK(kind<int>() == "int" && kind<float>() == "fp" && kind<std::string>() == "other");
    auto [x, y] = std::make_pair(3, 4.5);
    CHECK(x == 3 && y == 4.5);
    std::tuple<int, std::string, double> t(1, "two", 3.0);
    CHECK(std::get<1>(t) == "two" && std::tuple_size<decltype(t)>::value == 3);
    CHECK(std::apply([](int a, const std::string &b, double c) { return a + static_cast<int>(b.size()) + static_cast<int>(c); }, t) == 7);
    std::variant<int, double, std::string> var = 42;
    CHECK(std::visit(Visitor{}, var) == "i42");
    var = std::string("x");
    CHECK(std::visit(Visitor{}, var) == "sx" && var.index() == 2);
    var = 2.9;
    CHECK(std::visit(Visitor{}, var) == "d2");
    std::optional<int> o;
    CHECK(!o && o.value_or(5) == 5);
    o = 9;
    CHECK(o && *o == 9);
    std::any a = std::string("any");
    CHECK(std::any_cast<std::string>(a) == "any");
    a = 3;
    CHECK(std::any_cast<int>(a) == 3 && a.type() == typeid(int));
    int cap = 10;
    auto lam = [cap](int v) mutable { cap += v; return cap; };
    CHECK(lam(1) == 11 && lam(1) == 12 && cap == 10);
    std::function<int(int)> fact = [&fact](int n) { return n <= 1 ? 1 : n * fact(n - 1); };
    CHECK(fact(10) == 3628800);
    auto bound = std::bind([](int a, int b, int c) { return a * 100 + b * 10 + c; }, std::placeholders::_1, 2, std::placeholders::_2);
    CHECK(bound(1, 3) == 123);
    std::function<int(int)> compose = [](int v) { return v + 1; };
    auto twice = [&](int v) { return compose(compose(v)); };
    CHECK(twice(5) == 7);
    constexpr auto sq = make_sq();
    CHECK(sq[3] == 9);
    CHECK(std::numeric_limits<int>::max() == INT_MAX && std::numeric_limits<unsigned char>::digits == 8);
    CHECK(std::numeric_limits<double>::epsilon() == 2.220446049250313e-16 && std::numeric_limits<float>::max() > 3.4e38f);
    CHECK(std::numeric_limits<double>::infinity() > 1e308 && std::isnan(std::numeric_limits<double>::quiet_NaN()));
    CHECK(std::sqrt(2.0f) == 1.41421354f && std::abs(-3) == 3 && std::abs(-2.5) == 2.5 && std::pow(2, 10) == 1024.0);
    CHECK(std::hypot(3.0, 4.0) == 5.0 && std::fmod(7.5, 2.0) == 1.5 && std::floor(-1.5) == -2.0);
    CHECK(std::min({3, 1, 2}) == 1 && std::max(2.5, 1.5) == 2.5 && std::clamp(15, 0, 10) == 10);
}

// ------------------------------------------------------------------ memory, smart pointers, atomics
struct Node : std::enable_shared_from_this<Node> { int v; explicit Node(int x) : v(x) {} std::shared_ptr<Node> self() { return shared_from_this(); } };
struct alignas(32) Over32 { char c[33]; };
static int g_del;

static void t_memory()
{
    auto sp = std::make_shared<Node>(5);
    auto sp2 = sp->self();
    CHECK(sp.use_count() == 2 && sp2->v == 5);
    std::weak_ptr<Node> wp = sp;
    sp.reset();
    CHECK(!wp.expired());
    sp2.reset();
    CHECK(wp.expired());
    {
        std::unique_ptr<int, void (*)(int *)> up(new int(3), [](int *p) { ++g_del; delete p; });
        CHECK(*up == 3);
    }
    CHECK(g_del == 1);
    auto arr = std::make_unique<int[]>(100);
    arr[99] = 5;
    CHECK(arr[99] == 5 && arr[0] == 0);
    Over32 *o = new Over32;
    CHECK((reinterpret_cast<std::uintptr_t>(o) & 31) == 0);
    delete o;
    std::vector<Over32> ov(3);
    CHECK((reinterpret_cast<std::uintptr_t>(ov.data()) & 31) == 0);
    alignas(64) char buf[512];
    std::pmr::monotonic_buffer_resource mr(buf, sizeof buf);
    std::pmr::vector<int> pv(&mr);
    for (int i = 0; i < 50; i++) pv.push_back(i);
    CHECK(pv.size() == 50 && pv[49] == 49);
    std::atomic<int> ai{5};
    CHECK(ai.fetch_add(3) == 5 && ai.load() == 8);
    int expected = 8;
    CHECK(ai.compare_exchange_strong(expected, 20) && ai == 20);
    std::atomic<long long> al{1LL << 40};
    CHECK(al.fetch_add(5) == (1LL << 40) && al.load() == (1LL << 40) + 5);
    std::atomic<bool> flag{false};
    CHECK(!flag.exchange(true) && flag.load());
    static std::once_flag once;
    int ran = 0;
    std::call_once(once, [&] { ran++; });
    std::call_once(once, [&] { ran++; });
    CHECK(ran == 1);
}

// ------------------------------------------------------------------ <random> against the values the C++ standard publishes
static void t_random()
{
    std::mt19937 g;                                   // default seed 5489
    unsigned first = g();
    CHECK(first == 3499211612u);
    std::mt19937 g2;
    unsigned x = 0;
    for (int i = 0; i < 10000; i++) x = g2();
    CHECK(x == 4123659995u);
    std::minstd_rand0 m0;
    for (int i = 0; i < 10000; i++) x = static_cast<unsigned>(m0());
    CHECK(x == 1043618065u);
    std::minstd_rand m1;
    for (int i = 0; i < 10000; i++) x = static_cast<unsigned>(m1());
    CHECK(x == 399268537u);
    std::mt19937_64 g64;
    unsigned long long y = 0;
    for (int i = 0; i < 10000; i++) y = g64();
    CHECK(y == 9981545732273789042ull);
    std::mt19937 g3(12345);
    std::uniform_int_distribution<int> dist(1, 6);
    int counts[7] = {0};
    for (int i = 0; i < 6000; i++) counts[dist(g3)]++;
    bool ok = counts[0] == 0;
    for (int i = 1; i <= 6; i++) ok = ok && counts[i] > 800 && counts[i] < 1200;
    CHECK(ok);
    std::normal_distribution<double> nd(0.0, 1.0);
    double sum = 0;
    for (int i = 0; i < 4000; i++) sum += nd(g3);
    CHECK(std::fabs(sum / 4000) < 0.1);
}

// ------------------------------------------------------------------ chrono, locale, statics
static void t_runtime()
{
    CHECK(g_ctor_before_main == 17);
    using namespace std::chrono;
    CHECK(milliseconds(1500).count() == 1500 && duration_cast<seconds>(milliseconds(2500)).count() == 2);
    CHECK((seconds(2) + milliseconds(500)) == milliseconds(2500));
    auto t0 = steady_clock::now();
    volatile double sink = 0;
    for (int i = 0; i < 200000; i++) sink = sink + i * 0.5;
    auto t1 = steady_clock::now();
    CHECK(t1 >= t0);
    auto sys = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    INFOV(sys > 1600000000, "system_clock::now() = %lld s since the epoch (clock not set?)", static_cast<long long>(sys));
    std::locale loc = std::locale::classic();
    CHECK(loc.name() == "C");
    CHECK(std::isalpha('x', loc) && !std::isalpha('1', loc) && std::toupper('q', loc) == 'Q' && std::isdigit('7', loc));
    CHECK(std::use_facet<std::numpunct<char>>(loc).decimal_point() == '.');
    static int local_static_counter = 0;
    auto bump = [] { static int n = 100; return ++n + local_static_counter; };
    CHECK(bump() == 101 && bump() == 102);
    struct Guard { int *p; ~Guard() { *p += 1; } };
    int dcount = 0;
    { Guard g1{&dcount}; Guard g2{&dcount}; }
    CHECK(dcount == 2);
    std::string a = "abc";
    std::string b = std::move(a);
    CHECK(b == "abc");
    std::vector<std::string> vs;
    vs.push_back(std::move(b));
    CHECK(vs[0] == "abc");
    char *p = static_cast<char *>(std::malloc(10));
    CHECK(p != nullptr);
    std::free(p);
    CHECK(std::strtol("7fffffff", nullptr, 16) == 0x7fffffffL);
    std::srand(1);
    int rr = std::rand();
    CHECK(rr >= 0);
}

// ------------------------------------------------------------------ opt-in: threads
static void t_threads()
{
    std::atomic<int> counter{0};
    std::vector<std::thread> ts;
    for (int i = 0; i < 4; i++) ts.emplace_back([&counter] { for (int k = 0; k < 1000; k++) counter++; });
    for (auto &t : ts) t.join();
    CHECK(counter == 4000);
    std::mutex mu;
    int guarded = 0;
    std::thread a([&] { for (int i = 0; i < 500; i++) { std::lock_guard<std::mutex> lk(mu); guarded++; } });
    std::thread b([&] { for (int i = 0; i < 500; i++) { std::lock_guard<std::mutex> lk(mu); guarded++; } });
    a.join(); b.join();
    CHECK(guarded == 1000);
    auto fut = std::async(std::launch::async, [] { return 6 * 7; });
    CHECK(fut.get() == 42);
}

#define RUN(fn) do { int c0_ = g_checks, f0_ = g_fail; g_gf = 0; g_grp = #fn + 2; fn(); \
        std::printf("  %-10s %6d checks  %s\n", #fn + 2, g_checks - c0_, g_fail == f0_ ? "ok" : "FAILED"); } while (0)

int main(int argc, char **argv)
{
    bool threads = argc > 1 && std::strcmp(argv[1], "threads") == 0;
    std::printf("cxxtest 1.0 [%s] gcc %s\n", ROTEST_CFG, __VERSION__);
    RUN(t_exceptions); RUN(t_rtti); RUN(t_containers); RUN(t_strings); RUN(t_language); RUN(t_memory); RUN(t_random); RUN(t_runtime);
    if (threads) RUN(t_threads);
    std::printf("INFO: %d informational checks, %d differ\n", g_info, g_info_fail);
    std::printf("SUMMARY [%s]: %d checks, %d failed -> %s\n", ROTEST_CFG, g_checks, g_fail, g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}
