// cxx23.cc -- C++20/23 library and language features (GCC 14+/16 only).  Same output conventions as cxxtest.cc.
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <compare>
#include <concepts>
#include <cstdio>
#include <cstring>
#include <expected>
#if __has_include(<flat_map>)
#include <flat_map>
#define HAVE_FLAT_MAP 1
#endif
#include <format>
#if __has_include(<mdspan>)
#include <mdspan>
#define HAVE_MDSPAN 1
#endif
#include <numeric>
#include <optional>
#include <print>
#include <ranges>
#include <set>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifndef ROTEST_CFG
#define ROTEST_CFG "unknown"
#endif
static int g_checks, g_fail;
#define CHECK(c) do { g_checks++; if (!(c)) { g_fail++; std::printf("FAIL cxx23:%d %s\n", __LINE__, #c); } } while (0)

struct Version {
    int major, minor;
    auto operator<=>(const Version &) const = default;
};
template <typename T> concept Addable = requires(T a, T b) { a + b; };
template <Addable T> T add(T a, T b) { return a + b; }
consteval int sq(int x) { return x * x; }
static std::expected<int, std::string> parse(std::string_view s)
{
    int v = 0;
    for (char c : s) { if (c < '0' || c > '9') return std::unexpected(std::string("bad:") + c); v = v * 10 + (c - '0'); }
    return v;
}

int main()
{
    std::printf("cxx23 1.0 [%s] gcc %s\n", ROTEST_CFG, __VERSION__);
    // <format>
    CHECK(std::format("{} {}", 42, "x") == "42 x");
    CHECK(std::format("{:>8.3f}|{:<5}|{:^7}|{:#x}|{:08.2f}", 3.14159, 7, "mid", 255, -2.5) == "   3.142|7    |  mid  |0xff|-0002.50");
    CHECK(std::format("{:b} {:o} {:X} {:+d}", 10, 64, 255, 5) == "1010 100 FF +5");
    CHECK(std::format("{0} {1} {0}", "a", "b") == "a b a");
    CHECK(std::format("{:e}", 12345.678) == "1.234568e+04");
    CHECK(std::format("{}", 1.0 / 3.0) == "0.3333333333333333");
    CHECK(std::format("{:.0f} {:.1f}", 2.5, 0.25) == "2 0.2");
    CHECK(std::format("{}", std::string_view("sv")) == "sv" && std::format("{}", true) == "true");
    // ranges and views
    std::vector<int> v{5, 2, 8, 1, 9, 3};
    std::ranges::sort(v);
    CHECK(std::ranges::is_sorted(v));
    auto evens = v | std::views::filter([](int x) { return x % 2 == 0; }) | std::views::transform([](int x) { return x * 10; });
    std::vector<int> ev(evens.begin(), evens.end());
    CHECK(ev.size() == 2 && ev[0] == 20 && ev[1] == 80);
    CHECK(std::ranges::fold_left(v, 0, std::plus<>()) == 28);
    auto sq3 = std::views::iota(1, 6) | std::views::transform([](int x) { return x * x; });
    CHECK(std::ranges::fold_left(sq3, 0, std::plus<>()) == 55);
    auto joined = std::vector<std::string>{"a", "b", "c"} | std::views::join_with(',') | std::ranges::to<std::string>();
    CHECK(joined == "a,b,c");
    auto tk = std::views::iota(0) | std::views::take(5) | std::ranges::to<std::vector>();
    CHECK(tk.size() == 5 && tk[4] == 4);
    // span, concepts, consteval, <=>
    int arr[5] = {1, 2, 3, 4, 5};
    std::span<int> sp(arr);
    CHECK(sp.size() == 5 && sp.subspan(1, 3)[2] == 4 && sp.first(2)[1] == 2);
    CHECK(add(2, 3) == 5 && add(1.5, 2.0) == 3.5);
    constexpr int s9 = sq(9);
    CHECK(s9 == 81);
    Version a{1, 2}, b{1, 10};
    CHECK(a < b && a != b && (a <=> b) == std::strong_ordering::less);
    // expected / optional monadics
    CHECK(parse("123").value() == 123 && !parse("12x").has_value() && parse("12x").error() == "bad:x");
    CHECK(parse("7").transform([](int x) { return x * 2; }).value() == 14);
    std::optional<int> o = 4;
    CHECK(o.and_then([](int x) -> std::optional<int> { return x + 1; }).value() == 5);
    // bit utilities
    CHECK(std::popcount(0xF0F0u) == 8 && std::countl_zero(1u) == 31 && std::countr_zero(8u) == 3 && std::bit_width(255u) == 8);
    CHECK(std::rotl(0x80000001u, 1) == 3u && std::byteswap(0x11223344u) == 0x44332211u && std::has_single_bit(64u));
    float f = 1.5f;
    CHECK(std::bit_cast<unsigned>(f) == 0x3FC00000u);
    // containers new in C++23
#ifdef HAVE_FLAT_MAP
    std::flat_map<int, std::string> fm;
    fm[3] = "c"; fm[1] = "a"; fm[2] = "b";
    CHECK(fm.size() == 3 && fm.begin()->first == 1 && fm.at(2) == "b");
#endif
#ifdef HAVE_MDSPAN
    int grid[6] = {1, 2, 3, 4, 5, 6};
    std::mdspan<int, std::dextents<std::size_t, 2>> md(grid, 2, 3);
    CHECK((md[1, 2]) == 6 && md.extent(0) == 2 && md.extent(1) == 3);
#endif
    // calendar arithmetic (no time zones)
    using namespace std::chrono;
    year_month_day ymd{year(2024), month(2), day(28)};
    ymd = sys_days(ymd) + days(2);
    CHECK(ymd == year_month_day(year(2024), month(3), day(1)));
    CHECK(weekday(sys_days(year_month_day(year(2000), month(1), day(1)))) == Saturday);
    CHECK(static_cast<int>(year_month_day_last(year(2023), month_day_last(month(2))).day().operator unsigned()) == 28);
    auto loc = std::source_location::current();
    CHECK(loc.line() > 0 && std::strstr(loc.file_name(), "cxx23.cc") != nullptr);
    std::println("  [std::println ok] {} {:.2f}", "pi is", 3.14159);
    std::printf("SUMMARY [%s]: %d checks, %d failed -> %s\n", ROTEST_CFG, g_checks, g_fail, g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}
