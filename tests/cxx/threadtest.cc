// threadtest.cc -- C++ concurrency on RISC OS (UnixLib pthreads) with GCC 16's libstdc++.
//
//   threadtest list                      the test names
//   threadtest probe                     which libunixlib is installed (exit status 0 = the 10.2.0-5 fixes are there)
//   --need-fix (anywhere)                refuse to run anything unless the 10.2.0-5 libunixlib fixes are installed
//   threadtest <name> [<name> ...]       run those tests
//   threadtest all [-x a,b] [-s name]    run every test in order (-x: skip these, -s: start at this one)
//
// Each test prints one line "T <name>: ok" / "T <name>: FAIL <why>"; INFO lines describe behaviour that is allowed to
// differ between systems.  A thread deadlock makes UnixLib abort the whole program ("Deadlocked (no runnable threads)"),
// so the tests are ordered from the most to the least likely to work and the program flushes before every test: the
// last "RUN <name>" line tells which test it was.  Press Escape if a test hangs.
// Needs C++17; C++20 parts (latch, barrier, semaphore, jthread, atomic wait) are used when the library has them.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#if __cplusplus >= 202002L && __has_include(<latch>)
#include <latch>
#include <barrier>
#include <semaphore>
#include <stop_token>
#define HAVE_CXX20_SYNC 1
#endif

using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

static std::string g_why;
#define REQUIRE(c) do { if (!(c)) { g_why = std::string("line ") + std::to_string(__LINE__) + ": " + #c; return false; } } while (0)
static long ms_since(Clock::time_point t0) { return (long)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count(); }
static void info(const char *fmt, long a = 0, long b = 0) { std::printf("  INFO "); std::printf(fmt, a, b); std::printf("\n"); std::fflush(stdout); }

// ---------------------------------------------------------------------------------------------------------------
static bool t_basic()
{
    int x = 0;
    std::thread t([&] { x = 42; });
    REQUIRE(t.joinable());
    t.join();
    REQUIRE(!t.joinable() && x == 42);
    std::thread::id main_id = std::this_thread::get_id(), other{};
    std::thread u([&] { other = std::this_thread::get_id(); });
    u.join();
    REQUIRE(other != main_id && other != std::thread::id{});
    std::string s;
    std::thread v([](std::string &out, int n, const char *p) { out = std::string(p) + std::to_string(n); }, std::ref(s), 7, "abc");
    v.join();
    REQUIRE(s == "abc7");
    info("hardware_concurrency() = %ld", (long)std::thread::hardware_concurrency());
    return true;
}

static bool t_mutex()
{
    std::mutex m;
    int n = 0;
    std::vector<std::thread> ts;
    for (int i = 0; i < 4; i++) ts.emplace_back([&] { for (int k = 0; k < 2000; k++) { std::lock_guard<std::mutex> g(m); n++; } });
    for (auto &t : ts) t.join();
    REQUIRE(n == 8000);
    {   // try_lock from another thread fails while the lock is held here
        std::lock_guard<std::mutex> g(m);
        bool got = true;
        std::thread t([&] { got = m.try_lock(); if (got) m.unlock(); });
        t.join();
        REQUIRE(!got);
    }
    // std::scoped_lock takes two mutexes in opposite orders from two threads without deadlock
    std::mutex a, b; int c = 0;
    std::thread p([&] { for (int k = 0; k < 500; k++) { std::scoped_lock l(a, b); c++; } });
    std::thread q([&] { for (int k = 0; k < 500; k++) { std::scoped_lock l(b, a); c++; } });
    p.join(); q.join();
    REQUIRE(c == 1000);
    return true;
}

static bool t_recursive()
{
    std::recursive_mutex m;
    int depth = 0;
    std::function<void(int)> f = [&](int d) { std::lock_guard<std::recursive_mutex> g(m); depth = std::max(depth, d); if (d < 5) f(d + 1); };
    f(1);
    REQUIRE(depth == 5);
    bool got = true;
    {
        std::lock_guard<std::recursive_mutex> g(m);
        std::thread t([&] { got = m.try_lock(); if (got) m.unlock(); });
        t.join();
    }
    REQUIRE(!got);
    return true;
}

static bool t_condvar()
{
    std::mutex m; std::condition_variable cv; std::deque<int> q; bool done = false;
    long sum_in = 0; std::atomic<long> sum_out{0}; std::atomic<int> popped{0};
    std::vector<std::thread> prod, cons;
    for (int p = 0; p < 2; p++)
        prod.emplace_back([&, p] { for (int i = 1; i <= 300; i++) { { std::lock_guard<std::mutex> g(m); q.push_back(p * 1000 + i); } cv.notify_one(); } });
    for (int c = 0; c < 2; c++)
        cons.emplace_back([&] {
            for (;;) {
                std::unique_lock<std::mutex> l(m);
                cv.wait(l, [&] { return !q.empty() || done; });
                if (q.empty()) return;
                int v = q.front(); q.pop_front(); l.unlock();
                sum_out += v; popped++;
            }
        });
    for (int p = 0; p < 2; p++) for (int i = 1; i <= 300; i++) sum_in += p * 1000 + i;
    for (auto &t : prod) t.join();
    { std::lock_guard<std::mutex> g(m); done = true; }
    cv.notify_all();
    for (auto &t : cons) t.join();
    REQUIRE(popped == 600 && sum_out == sum_in);
    return true;
}

template <class C> static long tick_us()
{
    auto a = C::now(), b = a;
    for (int i = 0; i < 100000000 && b == a; i++) b = C::now();
    return (long)std::chrono::duration_cast<std::chrono::microseconds>(b - a).count();
}

static bool t_cv_timeout()
{
    info("clock ticks: steady_clock %ld us, system_clock %ld us", tick_us<std::chrono::steady_clock>(), tick_us<std::chrono::system_clock>());
    std::mutex m; std::condition_variable cv;
    std::unique_lock<std::mutex> l(m);
    auto t0 = Clock::now();
    auto st = cv.wait_for(l, 60ms);                       // nobody notifies: must time out (single thread in the process)
    long el = ms_since(t0);
    info("wait_for(60ms) returned after %ld ms", el);
    REQUIRE(st == std::cv_status::timeout && el >= 40 && el < 2000);
    t0 = Clock::now();
    bool ok = cv.wait_until(l, Clock::now() + 30ms, [] { return false; });
    el = ms_since(t0);
    REQUIRE(!ok && el >= 20 && el < 2000);
    t0 = Clock::now();
    st = cv.wait_until(l, std::chrono::system_clock::now() + 30ms);   // system_clock deadline
    el = ms_since(t0);
    info("wait_until(system_clock + 30ms) returned after %ld ms", el);
    REQUIRE(st == std::cv_status::timeout && el >= 20 && el < 2000);
    l.unlock();
    // while another thread is runnable
    std::atomic<bool> stop{false}; std::atomic<long> spins{0};
    std::thread w([&] { while (!stop) { spins++; std::this_thread::yield(); } });
    l.lock();
    st = cv.wait_for(l, 60ms);
    l.unlock();
    stop = true; w.join();
    REQUIRE(st == std::cv_status::timeout && spins > 0);
    return true;
}

static bool t_statics()
{
    struct Slow { int v; Slow() : v(7) { std::this_thread::yield(); } };
    static std::atomic<int> constructed{0};
    struct Counted { Counted() { constructed++; std::this_thread::yield(); } };
    auto get = []() -> int { static Counted c; (void)c; return 1; };
    std::vector<std::thread> ts;
    for (int i = 0; i < 4; i++) ts.emplace_back([&] { for (int k = 0; k < 50; k++) get(); });
    for (auto &t : ts) t.join();
    REQUIRE(constructed == 1);
    return true;
}

static bool t_malloc_mt()
{
    std::atomic<long> total{0};
    std::vector<std::thread> ts;
    for (int i = 0; i < 4; i++)
        ts.emplace_back([&, i] {
            unsigned seed = 12345u + i; long mine = 0; std::vector<char *> keep;
            for (int k = 0; k < 4000; k++) {
                seed = seed * 1103515245u + 12345u;
                size_t n = 1 + (seed >> 16) % 2000;
                char *p = new char[n];
                std::memset(p, (int)(n & 0xff), n);
                keep.push_back(p);
                if (keep.size() > 30) { char *q = keep[(seed >> 8) % keep.size()]; mine += (unsigned char)q[0]; delete[] q; keep.erase(std::find(keep.begin(), keep.end(), q)); }
            }
            for (char *p : keep) delete[] p;
            total += mine;
        });
    for (auto &t : ts) t.join();
    REQUIRE(total.load() >= 0);
    return true;
}

static bool t_exceptions_mt()
{
    struct E1 { int v; }; struct E2 : std::runtime_error { using std::runtime_error::runtime_error; };
    std::atomic<int> caught{0}; std::atomic<int> bad{0};
    auto thrower = [](int depth, int kind) {
        std::function<void(int)> f = [&](int d) { if (d == 0) { if (kind & 1) throw E1{d}; else throw E2("boom"); } f(d - 1); };
        f(depth);
    };
    std::vector<std::thread> ts;
    for (int i = 0; i < 4; i++)
        ts.emplace_back([&, i] {
            for (int k = 0; k < 400; k++) {
                try { thrower(3 + (k % 5), i + k); bad++; }
                catch (const E1 &e) { if (e.v == 0) caught++; else bad++; }
                catch (const std::exception &e) { if (std::strcmp(e.what(), "boom") == 0) caught++; else bad++; }
            }
        });
    for (auto &t : ts) t.join();
    REQUIRE(caught == 1600 && bad == 0);
    return true;
}

static bool t_many_threads()
{
    // progress lines so that a hang can be located (round 1: threadtest10 stopped after "RUN many_threads")
    int n = 0;
    auto t0 = Clock::now();
    for (int i = 0; i < 150; i++) {
        std::thread t([&] { n++; });
        t.join();
        if (i % 50 == 49) info("many_threads: %ld sequential create+join done, %ld ms", i + 1, ms_since(t0));
    }
    REQUIRE(n == 150);
    std::atomic<int> m{0};
    std::vector<std::thread> ts;
    t0 = Clock::now();
    for (int i = 0; i < 12; i++) ts.emplace_back([&] { for (int k = 0; k < 200; k++) { m++; std::this_thread::yield(); } });
    info("many_threads: 12 threads started, %ld ms", ms_since(t0));
    for (auto &t : ts) t.join();
    info("many_threads: 12 threads joined after %ld ms", ms_since(t0));
    REQUIRE(m == 2400);
    return true;
}

static bool t_yield_spin()
{
    std::atomic<bool> flag{false};
    std::thread t([&] { for (int i = 0; i < 2000; i++) __asm__ __volatile__("" ::: "memory"); flag = true; });
    long it = 0;
    while (!flag && it < 20000000) { std::this_thread::yield(); it++; }
    REQUIRE(flag);
    t.join();
    info("yield-spin: set after %ld iterations", it);
    // pure spin without yield: only informational (needs pre-emption); capped, then it yields explicitly
    std::atomic<bool> f2{false};
    std::thread u([&] { for (int i = 0; i < 2000; i++) __asm__ __volatile__("" ::: "memory"); f2 = true; });
    long it2 = 0; bool needed_yield = false;
    while (!f2) { if (++it2 > 3000000) { needed_yield = true; std::this_thread::yield(); } }
    u.join();
    if (needed_yield) std::printf("  INFO (a pure spin loop did NOT get pre-empted: explicit yield was needed)\n");
    else std::printf("  INFO (pure spin loop was pre-empted by the scheduler)\n");
    return true;
}

static bool t_shared_mutex()
{
    std::shared_mutex sm; int value = 0; std::atomic<int> bad{0};
    std::vector<std::thread> ts;
    for (int r = 0; r < 3; r++)
        ts.emplace_back([&] { for (int k = 0; k < 300; k++) { std::shared_lock<std::shared_mutex> l(sm); int a = value; std::this_thread::yield(); if (a != value) bad++; } });
    ts.emplace_back([&] { for (int k = 0; k < 300; k++) { std::unique_lock<std::shared_mutex> l(sm); value++; std::this_thread::yield(); } });
    for (auto &t : ts) t.join();
    REQUIRE(value == 300 && bad == 0);
    return true;
}

static bool t_promise()
{
    std::promise<int> p; std::future<int> f = p.get_future();
    std::thread t([&] { std::this_thread::yield(); p.set_value(99); });
    REQUIRE(f.get() == 99);
    t.join();
    std::promise<std::string> ps; std::shared_future<std::string> sf = ps.get_future().share();
    std::vector<std::thread> w; std::atomic<int> ok{0};
    for (int i = 0; i < 3; i++) w.emplace_back([&] { if (sf.get() == "shared") ok++; });
    std::this_thread::yield();
    ps.set_value("shared");
    for (auto &x : w) x.join();
    REQUIRE(ok == 3);
    std::packaged_task<int(int)> pt([](int x) { return x * 3; });
    auto pf = pt.get_future();
    std::thread run(std::move(pt), 14);
    REQUIRE(pf.get() == 42);
    run.join();
    std::promise<int> pe; auto fe = pe.get_future();
    std::thread te([&] { try { throw std::runtime_error("from thread"); } catch (...) { pe.set_exception(std::current_exception()); } });
    bool threw = false;
    try { fe.get(); } catch (const std::runtime_error &e) { threw = std::strcmp(e.what(), "from thread") == 0; }
    te.join();
    REQUIRE(threw);
    return true;
}

static bool t_call_once()
{
    std::once_flag fl; std::atomic<int> runs{0};
    std::vector<std::thread> ts;
    for (int i = 0; i < 4; i++) ts.emplace_back([&] { for (int k = 0; k < 20; k++) std::call_once(fl, [&] { runs++; std::this_thread::yield(); }); });
    for (auto &t : ts) t.join();
    REQUIRE(runs == 1);
    // the std::async shape: one thread runs call_once (join) while the thread it joins needs ANOTHER once_flag
    std::once_flag fa, fb; std::atomic<int> b_ran{0};
    std::thread worker([&] { std::this_thread::yield(); std::call_once(fb, [&] { b_ran = 1; }); });
    std::call_once(fa, [&] { worker.join(); });
    REQUIRE(b_ran == 1);
    return true;
}

// A C++ exception thrown by the callable must propagate through pthread_once() to the caller, and the flag must stay
// unset so that the next call runs the callable again.  (UnixLib's pthread_once had no unwind information: the exception
// ended the program with "terminate called after throwing an instance of 'int'".)  Last in the list for that reason.
static bool t_call_once_throw()
{
    std::once_flag f2; int tries = 0; bool caught = false;
    try { std::call_once(f2, [&] { tries++; throw 1; }); } catch (int) { caught = true; }
    REQUIRE(caught && tries == 1);
    std::call_once(f2, [&] { tries++; });
    REQUIRE(tries == 2);
    std::call_once(f2, [&] { tries++; });                     // already done: must not run again
    REQUIRE(tries == 2);
    // threads waiting while the running callable throws: one of them must take over; everybody must finish
    for (int round = 0; round < 5; round++) {
        std::once_flag f; std::atomic<int> successes{0}, throws{0};
        std::vector<std::thread> ts;
        for (int i = 0; i < 4; i++) ts.emplace_back([&] {
            try { std::call_once(f, [&] { std::this_thread::yield(); if (successes + throws < 2) { throws++; throw 3; } successes++; }); }
            catch (int) {}
        });
        for (auto &t : ts) t.join();
        REQUIRE(successes == 1 && throws == 2);
    }
    return true;
}

static bool t_async()
{
    auto f = std::async(std::launch::async, [] { return 6 * 7; });
    REQUIRE(f.get() == 42);
    std::vector<std::future<int>> v;
    for (int i = 0; i < 6; i++) v.push_back(std::async(std::launch::async, [i] { std::this_thread::yield(); return i * i; }));
    int sum = 0; for (auto &x : v) sum += x.get();
    REQUIRE(sum == 55);
    auto e = std::async(std::launch::async, []() -> int { throw std::logic_error("async threw"); });
    bool threw = false;
    try { e.get(); } catch (const std::logic_error &x) { threw = std::strcmp(x.what(), "async threw") == 0; }
    REQUIRE(threw);
    auto d = std::async(std::launch::deferred, [] { return 5; });
    REQUIRE(d.wait_for(0ms) == std::future_status::deferred && d.get() == 5);
    return true;
}

static bool t_timed_mutex()
{
    std::timed_mutex tm;
    REQUIRE(tm.try_lock_for(10ms));
    tm.unlock();
    tm.lock();
    bool got = true; long el = 0;
    std::thread t([&] { auto t0 = Clock::now(); got = tm.try_lock_for(40ms); el = ms_since(t0); if (got) tm.unlock(); });
    t.join();
    tm.unlock();
    info("try_lock_for(40ms) on a held mutex returned after %ld ms", el);
    REQUIRE(!got && el >= 25 && el < 2000);
    std::shared_timed_mutex stm;
    stm.lock();
    bool sgot = true; long sel = 0;
    std::thread u([&] { auto t0 = Clock::now(); sgot = stm.try_lock_shared_for(30ms); sel = ms_since(t0); if (sgot) stm.unlock_shared(); });
    u.join();
    stm.unlock();
    info("shared_timed_mutex::try_lock_shared_for(30ms) on a writer-held mutex returned after %ld ms", sel);
    REQUIRE(!sgot && sel >= 15 && sel < 2000);
    std::recursive_timed_mutex rtm;
    rtm.lock();
    bool rgot = true;
    std::thread w([&] { rgot = rtm.try_lock_for(30ms); if (rgot) rtm.unlock(); });
    w.join();
    REQUIRE(!rgot && rtm.try_lock_for(10ms));         // the owner can still take it again
    rtm.unlock(); rtm.unlock();
    return true;
}

// POSIX level: pthread_cond_timedwait itself, with both condition-variable clocks.  UnixLib used to work the timeout out
// from time(), i.e. whole seconds, so every timed wait overran by the fraction of the current second (up to a second),
// and its CLOCK_MONOTONIC branch added clock() to a time that was already absolute.
static timespec ts_after(clockid_t id, long ms)
{
    timespec t;
    clock_gettime(id, &t);
    t.tv_sec += ms / 1000;
    t.tv_nsec += (ms % 1000) * 1000000L;
    while (t.tv_nsec >= 1000000000L) { t.tv_nsec -= 1000000000L; t.tv_sec++; }
    while (t.tv_nsec < 0) { t.tv_nsec += 1000000000L; t.tv_sec--; }
    return t;
}

static bool pthread_wait_case(clockid_t id)
{
    const bool mono = id == CLOCK_MONOTONIC;
    pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cv;
    pthread_condattr_t at;
    REQUIRE(pthread_condattr_init(&at) == 0);
    REQUIRE(pthread_condattr_setclock(&at, id) == 0);
    REQUIRE(pthread_cond_init(&cv, &at) == 0);
    REQUIRE(pthread_mutex_lock(&m) == 0);
    // timeouts that end at different phases of the clock's whole seconds
    static const long waits[] = {80, 130, 170, 90, 110, 150};
    long worst = 0;
    for (long w : waits) {
        timespec abs = ts_after(id, w);
        auto t0 = Clock::now();
        int r = pthread_cond_timedwait(&cv, &m, &abs);
        long el = ms_since(t0);
        if (el - w > worst) worst = el - w;
        REQUIRE(r == ETIMEDOUT);
        REQUIRE(el >= w - 15);                       // never early (the clock ticks every 10 ms here)
        REQUIRE(el < w + 300);                       // the old code was up to 1000 ms late
    }
    info(mono ? "CLOCK_MONOTONIC condvar: 6 timed waits of 80..170 ms, worst overrun %ld ms"
              : "CLOCK_REALTIME condvar: 6 timed waits of 80..170 ms, worst overrun %ld ms", worst);
    // a deadline that has already passed returns at once
    timespec past = ts_after(id, -5);
    auto t0 = Clock::now();
    int r = pthread_cond_timedwait(&cv, &m, &past);
    REQUIRE(r == ETIMEDOUT && ms_since(t0) < 100);
    // a signal ends the wait early
    bool flag = false;
    std::thread s([&] { pthread_mutex_lock(&m); flag = true; pthread_cond_signal(&cv); pthread_mutex_unlock(&m); });
    timespec far = ts_after(id, 5000);
    r = 0; t0 = Clock::now();
    while (!flag && r == 0) r = pthread_cond_timedwait(&cv, &m, &far);
    long el = ms_since(t0);
    pthread_mutex_unlock(&m);
    s.join();
    REQUIRE(flag && r == 0 && el < 3000);
    pthread_cond_destroy(&cv); pthread_condattr_destroy(&at);
    return true;
}

static bool t_pthread_timedwait()
{
    if (!pthread_wait_case(CLOCK_REALTIME)) return false;
    return pthread_wait_case(CLOCK_MONOTONIC);
}

static bool t_future_wait_for()
{
    std::promise<int> p; std::future<int> f = p.get_future();
    auto t0 = Clock::now();
    auto st = f.wait_for(50ms);                              // not set yet
    long el = ms_since(t0);
    info("future::wait_for(50ms) on an unset promise returned after %ld ms", el);
    REQUIRE(st == std::future_status::timeout && el >= 35 && el < 1000);
    t0 = Clock::now();
    st = f.wait_until(std::chrono::system_clock::now() + 40ms);   // a system_clock deadline takes the other path
    el = ms_since(t0);
    REQUIRE(st == std::future_status::timeout && el >= 25 && el < 1000);
    std::thread t([&] { std::this_thread::yield(); p.set_value(3); });
    st = f.wait_for(5s);
    REQUIRE(st == std::future_status::ready);
    t.join();
    REQUIRE(f.get() == 3);
    // a task on its own thread that finishes in time
    auto g = std::async(std::launch::async, [] { return 9; });
    REQUIRE(g.wait_for(5s) == std::future_status::ready && g.get() == 9);
    return true;
}

static bool t_sleep_for()
{
    // Three threads and main sleep at the same time.  Every sleeper measures its own sleep, which must not end early
    // (UnixLib used to share ONE alarm between all sleepers: the shortest sleeper's alarm replaced the others' and woke
    // them all), and the sleeps must overlap rather than add up.
    struct Sleeper { long want; long got; };
    Sleeper s[3] = {{100, -1}, {160, -1}, {220, -1}};
    auto t0 = Clock::now();
    std::vector<std::thread> ts;
    for (auto &x : s) ts.emplace_back([&x] { auto a = Clock::now(); std::this_thread::sleep_for(std::chrono::milliseconds(x.want)); x.got = ms_since(a); });
    for (int i = 0; i < 3; i++) std::this_thread::sleep_for(40ms);        // main sleeps as well
    long main_el = ms_since(t0);
    for (auto &t : ts) t.join();
    long el = ms_since(t0);
    info("three sleepers of 100/160/220 ms measured %ld ms for the first (second and third follow)", s[0].got);
    info("... %ld ms for the second, %ld ms for the third", s[1].got, s[2].got);
    info("main slept 3 x 40 ms in %ld ms; the whole group took %ld ms (about 230 = they overlap, about 600 = they serialise)", main_el, el);
    for (auto &x : s) REQUIRE(x.got >= x.want - 15);
    REQUIRE(main_el >= 105);
    REQUIRE(el >= 205 && el < 3000);
    // sleep_until
    t0 = Clock::now();
    std::this_thread::sleep_until(t0 + 50ms);
    REQUIRE(ms_since(t0) >= 35 && ms_since(t0) < 2000);
    // a worker sleeping while main waits in join(): the scheduler must not call this a deadlock
    std::atomic<int> v{0};
    std::thread b([&] { std::this_thread::sleep_for(50ms); v = 1; });
    b.join();
    REQUIRE(v == 1);
    return true;
}

#ifdef HAVE_CXX20_SYNC
static bool t_atomic_wait()
{
    std::atomic<int> turn{0}; int rounds = 400; int ping = 0, pong = 0;
    std::thread t([&] { for (int i = 0; i < rounds; i++) { turn.wait(0); pong++; turn.store(0); turn.notify_one(); } });
    for (int i = 0; i < rounds; i++) { turn.store(1); turn.notify_one(); turn.wait(1); ping++; }
    t.join();
    REQUIRE(ping == rounds && pong == rounds);
    return true;
}

static bool t_latch_barrier()
{
    std::latch l(4); std::atomic<int> arrived{0};
    std::vector<std::thread> ts;
    for (int i = 0; i < 4; i++) ts.emplace_back([&] { arrived++; l.count_down(); });
    l.wait();
    REQUIRE(arrived == 4);
    for (auto &t : ts) t.join();
    std::atomic<int> phase_done{0}; std::atomic<int> completions{0};
    std::barrier b(3, [&]() noexcept { completions++; });
    std::vector<std::thread> bs;
    for (int i = 0; i < 3; i++) bs.emplace_back([&] { for (int p = 0; p < 5; p++) { phase_done++; b.arrive_and_wait(); } });
    for (auto &t : bs) t.join();
    REQUIRE(completions == 5 && phase_done == 15);
    return true;
}

static bool t_semaphore()
{
    std::counting_semaphore<8> sem(0); std::atomic<int> got{0};
    std::thread c([&] { for (int i = 0; i < 100; i++) { sem.acquire(); got++; } });
    for (int i = 0; i < 100; i++) { sem.release(); if (i % 10 == 0) std::this_thread::yield(); }
    c.join();
    REQUIRE(got == 100);
    std::binary_semaphore bs(0); int x = 0;
    std::thread t([&] { bs.acquire(); x = 5; });
    bs.release(); t.join();
    REQUIRE(x == 5);
    return true;
}

static bool t_semaphore_timed()
{
    std::binary_semaphore ts(0);
    auto t0 = Clock::now();
    bool ok = ts.try_acquire_for(40ms);
    long el = ms_since(t0);
    info("try_acquire_for(40ms) on an empty semaphore returned after %ld ms", el);
    REQUIRE(!ok && el >= 25 && el < 2000);
    return true;
}

static bool t_jthread()
{
    std::atomic<long> loops{0};
    {
        std::jthread jt([&](std::stop_token st) { while (!st.stop_requested()) { loops++; std::this_thread::yield(); } });
        while (loops < 5) std::this_thread::yield();
        jt.request_stop();
    }                                                                  // joins here
    REQUIRE(loops >= 5);
    std::atomic<int> cb{0};
    std::stop_source ss;
    std::stop_callback c(ss.get_token(), [&] { cb++; });
    ss.request_stop();
    REQUIRE(cb == 1);
    std::mutex m; std::condition_variable_any cva; bool ready = false; bool stopped_wait = false;
    std::jthread w([&](std::stop_token st) { std::unique_lock<std::mutex> l(m); stopped_wait = !cva.wait(l, st, [&] { return ready; }); });
    std::this_thread::yield();
    w.request_stop();
    w.join();
    REQUIRE(stopped_wait);
    return true;
}
#endif

// ---------------------------------------------------------------------------------------------------------------
struct Test { const char *name; bool (*fn)(); };
static const Test tests[] = {
    // most likely to work first; the ones that depend on timed waits / sleeping come last
    {"basic", t_basic}, {"mutex", t_mutex}, {"recursive", t_recursive}, {"condvar", t_condvar}, {"statics", t_statics},
    {"malloc_mt", t_malloc_mt}, {"exceptions_mt", t_exceptions_mt}, {"many_threads", t_many_threads},
    {"shared_mutex", t_shared_mutex}, {"promise", t_promise}, {"yield_spin", t_yield_spin},
#ifdef HAVE_CXX20_SYNC
    {"atomic_wait", t_atomic_wait}, {"latch_barrier", t_latch_barrier}, {"semaphore", t_semaphore}, {"jthread", t_jthread},
#endif
    {"cv_timeout", t_cv_timeout}, {"timed_mutex", t_timed_mutex}, {"pthread_timedwait", t_pthread_timedwait},
    {"future_wait_for", t_future_wait_for},
#ifdef HAVE_CXX20_SYNC
    {"semaphore_timed", t_semaphore_timed},
#endif
    {"call_once", t_call_once}, {"async", t_async},     // both deadlock with UnixLib's original pthread_once
    {"sleep_for", t_sleep_for},
    {"call_once_throw", t_call_once_throw},         // ends the program on a UnixLib whose pthread_once has no unwind info
};

static bool run_one(const Test &t)
{
    std::printf("RUN %s\n", t.name); std::fflush(stdout);
    g_why.clear();
    bool ok = false;
    try { ok = t.fn(); } catch (const std::exception &e) { g_why = std::string("uncaught exception: ") + e.what(); } catch (...) { g_why = "uncaught exception"; }
    std::printf("T %s: %s%s%s\n", t.name, ok ? "ok" : "FAIL", ok ? "" : " ", ok ? "" : g_why.c_str());
    std::fflush(stdout);
    return ok;
}

// Which libunixlib is installed?  The 10.2.0-5 (and later) SharedLibs-C-armeabihf packages answer the private selector
// sysconf (0x4700) with their fix level (5); -4 only has sysconf (28) == 1; the stock 10.2.0-1 and the round-1 -3 answer -1.
static const long kNeedFixLevel = 5;
static long unixlib_fix_level()
{
#ifdef __riscos__
    return sysconf(0x4700);
#else
    return kNeedFixLevel;                                   // not RISC OS: nothing to check
#endif
}

static void print_probe()
{
#ifdef __riscos__
    long lvl = unixlib_fix_level(), nproc = sysconf(28);
    if (lvl >= kNeedFixLevel)
        std::printf("UnixLib: fix level %ld (%s) -- OK\n", lvl,
                    lvl == 5 ? "SharedLibs-C-armeabihf 10.2.0-5, or 16.2.0-1/-2" : lvl == 6 ? "SharedLibs-C-armeabihf 16.2.0-4" : "SharedLibs-C-armeabihf 16.2.0-5 or later");
    else if (nproc == 1) std::printf("UnixLib: the 10.2.0-4 package (fix level 4): timed waits and sleep are fixed, pthread_once is not exception-safe\n");
    else std::printf("UnixLib: NO FIX LEVEL -- the stock 10.2.0-1 or the round-1 10.2.0-3 package is installed; install SharedLibs-C-armeabihf_10.2.0-5\n");
#else
    std::printf("UnixLib: not RISC OS\n");
#endif
    std::fflush(stdout);
}

int main(int argc, char **argv)
{
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    bool need_fix = false;
    { int n = 1; for (int i = 1; i < argc; i++) { if (!std::strcmp(argv[i], "--need-fix")) need_fix = true; else argv[n++] = argv[i]; } argc = n; }
    std::printf("threadtest 1.4 [%s] gcc %s\n",
#ifdef ROTEST_CFG
                ROTEST_CFG,
#else
                "unknown",
#endif
                __VERSION__);
    print_probe();
    if (argc >= 2 && !std::strcmp(argv[1], "probe")) return unixlib_fix_level() >= kNeedFixLevel ? 0 : 2;
    if (need_fix && unixlib_fix_level() < kNeedFixLevel) {
        std::printf("*** The UnixLib fix package is not installed: nothing was run.  Install SharedLibs-C-armeabihf_10.2.0-5_arm.zip,a91\n"
                    "*** with PackMan (not an older -1/-3/-4 file), then run this again.  (*threadtest16 probe checks the installed library.)\n");
        return 2;
    }
    if (argc < 2 || !std::strcmp(argv[1], "list")) { for (auto &t : tests) std::printf("%s\n", t.name); return 0; }
    int passed = 0, failed = 0;
    if (!std::strcmp(argv[1], "all")) {
        std::string skip, start;
        for (int i = 2; i + 1 < argc; i += 2) { if (!std::strcmp(argv[i], "-x")) skip = "," + std::string(argv[i + 1]) + ","; else if (!std::strcmp(argv[i], "-s")) start = argv[i + 1]; }
        bool started = start.empty();
        for (auto &t : tests) {
            if (!started && start == t.name) started = true;
            if (!started) continue;
            if (!skip.empty() && skip.find(std::string(",") + t.name + ",") != std::string::npos) { std::printf("SKIP %s\n", t.name); continue; }
            (run_one(t) ? passed : failed)++;
        }
    } else {
        for (int i = 1; i < argc; i++) {
            bool found = false;
            for (auto &t : tests) if (!std::strcmp(argv[i], t.name)) { found = true; (run_one(t) ? passed : failed)++; }
            if (!found) { std::printf("unknown test %s\n", argv[i]); failed++; }
        }
    }
    std::printf("SUMMARY [threadtest]: %d passed, %d failed -> %s\n", passed, failed, failed ? "FAIL" : "PASS");
    return failed ? 1 : 0;
}
