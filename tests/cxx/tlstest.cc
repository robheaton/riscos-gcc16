// tlstest.cc -- thread_local / _Thread_local runtime checks (emulated TLS on RISC OS: libgcc's __emutls_get_address).
// Run with no argument for the main-thread checks; with "threads" for the pthread part.
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#ifndef ROTEST_CFG
#define ROTEST_CFG "unknown"
#endif
extern "C" int c_tls_bump(void);                      // _Thread_local in tlsc.c
static int g_checks, g_fail;
#define CHECK(c) do { g_checks++; if (!(c)) { g_fail++; std::printf("FAIL tls:%d %s\n", __LINE__, #c); } } while (0)

struct TL {
    int v = 11;
    std::string s = "init";
    ~TL() { std::printf("  (thread_local destructor ran: v=%d s=%s)\n", v, s.c_str()); }
};
thread_local TL tl;
thread_local int counter = 0;
thread_local int init_from_call = 40 + 2;
static int bump() { return ++counter; }

int main(int argc, char **argv)
{
    std::printf("tlstest 1.0 [%s] gcc %s\n", ROTEST_CFG, __VERSION__);
    CHECK(bump() == 1 && bump() == 2 && counter == 2);
    CHECK(init_from_call == 42);
    tl.v += 1;
    tl.s += "x";
    CHECK(tl.v == 12 && tl.s == "initx");
    CHECK(c_tls_bump() == 8 && c_tls_bump() == 9);
    std::printf("  main-thread checks done: %d checks, %d failed\n", g_checks, g_fail);
    if (argc > 1 && std::strcmp(argv[1], "threads") == 0) {
        std::atomic<int> ok{0};
        std::vector<std::thread> ts;
        for (int i = 0; i < 4; i++)
            ts.emplace_back([&ok] { if (bump() == 1 && bump() == 2 && tl.v == 11 && tl.s == "init" && c_tls_bump() == 8 && init_from_call == 42) ok++; });
        for (auto &t : ts) t.join();
        CHECK(ok == 4);
        CHECK(counter == 2 && tl.v == 12 && c_tls_bump() == 10);
        std::printf("  thread checks done\n");
    }
    std::printf("SUMMARY [%s]: %d checks, %d failed -> %s\n", ROTEST_CFG, g_checks, g_fail, g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}
