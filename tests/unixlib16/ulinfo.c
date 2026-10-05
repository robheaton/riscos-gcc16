/* ulinfo.c -- which libunixlib is installed, and a smoke test of the C library services most programs rely on.
   Built for RISC OS with GCC 16.2; also builds and passes natively on Linux (the sysconf selectors are then unknown).  */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <fenv.h>
#include <math.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <unistd.h>

#ifndef ROTEST_CFG
#define ROTEST_CFG "ulinfo"
#endif

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("  FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static int cmp_int(const void *a, const void *b) { int x = *(const int *) a, y = *(const int *) b; return (x > y) - (x < y); }

static double __attribute__((noinline, noipa)) divide(double a, double b) { return a / b; }

static void test_numbers(void)
{
  char buf[64];
  double d = 1.0 / 3.0;
  snprintf(buf, sizeof buf, "%.10g|%5.2f|%e|%x|%lld|%08.3f", d, 3.14159, 12345.678, 255u, 1234567890123LL, -2.5);
  CHECK(strcmp(buf, "0.3333333333| 3.14|1.234568e+04|ff|1234567890123|-002.500") == 0);
  CHECK(strtod("1.5e3", NULL) == 1500.0 && strtod("0x1p4", NULL) == 16.0 && strtol("-0x1f", NULL, 0) == -31);
  CHECK(strtoull("18446744073709551615", NULL, 10) == 18446744073709551615ULL);
  volatile long long a = 1LL << 40, b = 3;
  CHECK(a / b == 366503875925LL && a % b == 1 && (-a) / b == -366503875925LL);
  volatile unsigned long long ua = 0xFFFFFFFFFFFFFFFFULL;
  CHECK(ua / 10 == 1844674407370955161ULL && ua % 10 == 5);
  int v; unsigned u; double x;
  CHECK(sscanf("42 0x2A 2.5", "%d %x %lf", &v, &u, &x) == 3 && v == 42 && u == 42 && x == 2.5);
  CHECK(atoi("  -17x") == -17 && fabs(atof("2.75") - 2.75) < 1e-15);
  CHECK(sin(1.0) > 0.8414709848 && sin(1.0) < 0.8414709849 && fabs(exp(1.0) - 2.718281828459045) < 1e-15);
  CHECK(pow(2.0, 0.5) > 1.41421356 && fmod(7.5, 2.0) == 1.5 && floor(-2.5) == -3.0 && ceil(-2.5) == -2.0 && lround(2.5) == 3);
  CHECK(sqrtf(2.0f) >= 1.4142135f && sqrtf(2.0f) <= 1.4142137f && fabsf(-3.0f) == 3.0f && hypot(3, 4) == 5.0);
  CHECK(isnan(NAN) && isinf(INFINITY) && !isfinite(HUGE_VAL) && copysign(1.0, -0.0) == -1.0);
  /* the divisions must really happen between the fesetround calls: without -frounding-math GCC moves a plain a / b (it sank both below the last call) */
  volatile double one = 1.0, three = 3.0;
  fesetround(FE_DOWNWARD); double lo = divide(one, three);
  fesetround(FE_UPWARD);   double hi = divide(one, three);
  fesetround(FE_TONEAREST);
  CHECK(lo < hi && fegetround() == FE_TONEAREST && divide(one, three) == 1.0 / 3.0);
}

static void test_memory_strings(void)
{
  int arr[1000];
  for (int i = 0; i < 1000; i++) arr[i] = (i * 7919) % 1000;
  qsort(arr, 1000, sizeof arr[0], cmp_int);
  int ok = 1; for (int i = 1; i < 1000; i++) if (arr[i - 1] > arr[i]) ok = 0;
  CHECK(ok);
  int key = 500; CHECK(bsearch(&key, arr, 1000, sizeof arr[0], cmp_int) != NULL);
  /* allocator stress: 20000 blocks of varying size with patterns, realloc, free in a different order */
  enum { N = 20000 };
  unsigned char **p = malloc(N * sizeof *p); size_t *sz = malloc(N * sizeof *sz);
  CHECK(p && sz);
  unsigned seed = 12345;
  for (int i = 0; i < N; i++) { seed = seed * 1103515245 + 12345; sz[i] = 1 + (seed >> 16) % 300; p[i] = malloc(sz[i]); memset(p[i], i & 0xFF, sz[i]); }
  for (int i = 0; i < N; i += 3) { p[i] = realloc(p[i], sz[i] * 2); memset(p[i] + sz[i], 0x5A, sz[i]); }
  int bad = 0;
  for (int i = 0; i < N; i++) { for (size_t k = 0; k < sz[i]; k++) if (p[i][k] != (i & 0xFF)) bad++; if (i % 3 == 0) for (size_t k = sz[i]; k < sz[i] * 2; k++) if (p[i][k] != 0x5A) bad++; }
  CHECK(bad == 0);
  for (int i = 0; i < N; i += 2) free(p[i]);
  for (int i = 1; i < N; i += 2) free(p[i]);
  free(p); free(sz);
  char s[64] = "abcdefghij";
  memmove(s + 2, s, 8);
  CHECK(strcmp(s, "ababcdefgh") == 0 && strstr("hello world", "o w") != NULL && strchr("abc", 'c') != NULL && strrchr("a/b/c", '/')[1] == 'c');
  char t[] = "a,b;c"; char *save, *tok = strtok_r(t, ",;", &save); int n = 0; while (tok) { n++; tok = strtok_r(NULL, ",;", &save); }
  CHECK(n == 3 && strncmp("abcd", "abxx", 2) == 0 && strlen("") == 0 && strcasecmp("HeLLo", "hello") == 0);
  void *big = calloc(1, 4 * 1024 * 1024); CHECK(big != NULL && ((char *) big)[4 * 1024 * 1024 - 1] == 0); free(big);
}

static jmp_buf jb;
static volatile int got_sig, atexit_order;
static void on_sig(int s) { got_sig = s; }
static void on_exit1(void) { atexit_order = atexit_order * 10 + 1; }
static void on_exit2(void) { atexit_order = atexit_order * 10 + 2; }
static void *worker(void *arg) { pthread_mutex_t *m = arg; extern long counter_; for (int i = 0; i < 1000; i++) { pthread_mutex_lock(m); counter_++; pthread_mutex_unlock(m); } return NULL; }
long counter_;

static void test_process(void)
{
  if (setjmp(jb) == 0) longjmp(jb, 7); else CHECK(1);
  int r = setjmp(jb); if (r == 0) longjmp(jb, 3); CHECK(r == 3);
  signal(SIGUSR1, on_sig); raise(SIGUSR1); CHECK(got_sig == SIGUSR1);
  atexit(on_exit1); atexit(on_exit2);
  setenv("ULINFO_TEST", "xyz", 1); CHECK(getenv("ULINFO_TEST") && strcmp(getenv("ULINFO_TEST"), "xyz") == 0); unsetenv("ULINFO_TEST"); CHECK(getenv("ULINFO_TEST") == NULL);
  char cwd[512]; CHECK(getcwd(cwd, sizeof cwd) != NULL && strlen(cwd) > 0);
  time_t t1 = time(NULL); struct timeval tv; gettimeofday(&tv, NULL); clock_t c1 = clock(); struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  CHECK(t1 > 1700000000 && llabs((long long) tv.tv_sec - t1) <= 1 && c1 >= 0 && ts.tv_nsec >= 0 && ts.tv_nsec < 1000000000);
  struct timespec req = { 0, 50 * 1000 * 1000 }; clock_gettime(CLOCK_MONOTONIC, &ts); long long before = ts.tv_sec * 1000000000LL + ts.tv_nsec;
  nanosleep(&req, NULL); clock_gettime(CLOCK_MONOTONIC, &ts); long long el = ts.tv_sec * 1000000000LL + ts.tv_nsec - before;
  CHECK(el >= 40 * 1000000LL && el < 500 * 1000000LL);
  pthread_t th[4]; pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER; counter_ = 0;
  for (int i = 0; i < 4; i++) CHECK(pthread_create(&th[i], NULL, worker, &m) == 0);
  for (int i = 0; i < 4; i++) pthread_join(th[i], NULL);
  CHECK(counter_ == 4000);
}

static void test_files(void)
{
  const char *f1 = "ulinfo_tmp1", *f2 = "ulinfo_tmp2", *dir = "ulinfo_dir";
  unsigned char block[1024]; int ok = 1;
  FILE *f = fopen(f1, "wb"); CHECK(f != NULL); if (!f) return;
  for (int i = 0; i < 100; i++) { for (int k = 0; k < 1024; k++) block[k] = (unsigned char) (i * 31 + k); if (fwrite(block, 1, 1024, f) != 1024) ok = 0; }
  CHECK(fclose(f) == 0 && ok);
  struct stat st; CHECK(stat(f1, &st) == 0 && st.st_size == 102400 && S_ISREG(st.st_mode));
  f = fopen(f1, "rb"); CHECK(f != NULL); if (!f) return;
  fseek(f, 50 * 1024, SEEK_SET); CHECK(fread(block, 1, 1024, f) == 1024 && block[0] == (unsigned char) (50 * 31) && ftell(f) == 51 * 1024);
  fseek(f, -1, SEEK_END); CHECK(fgetc(f) == (unsigned char) (99 * 31 + 1023) && fgetc(f) == EOF && feof(f));
  fclose(f);
  CHECK(rename(f1, f2) == 0 && stat(f1, &st) != 0 && stat(f2, &st) == 0);
  f = fopen(f2, "a"); fputs("tail\n", f); fclose(f);
  CHECK(stat(f2, &st) == 0 && st.st_size == 102405);
  CHECK(mkdir(dir, 0777) == 0);
  DIR *d = opendir("."); int found = 0; struct dirent *e;
  if (d) { while ((e = readdir(d)) != NULL) if (strncmp(e->d_name, "ulinfo_", 7) == 0) found++; closedir(d); }
  CHECK(found >= 2);
  CHECK(rmdir(dir) == 0 && unlink(f2) == 0 && stat(f2, &st) != 0);
  int fd = open(f1, O_RDWR | O_CREAT | O_TRUNC, 0644); CHECK(fd >= 0);
  if (fd >= 0) { CHECK(write(fd, "0123456789", 10) == 10 && lseek(fd, 3, SEEK_SET) == 3); char b[4] = {0}; CHECK(read(fd, b, 3) == 3 && strcmp(b, "345") == 0); CHECK(ftruncate(fd, 5) == 0 && lseek(fd, 0, SEEK_END) == 5); close(fd); }
  unlink(f1);
  CHECK(access("ulinfo_does_not_exist", F_OK) != 0 && errno == ENOENT);
}

/* fread() into a big buffer on a stack the program has not touched yet.  On the user's machine the OS loses the store that takes the page fault when IT is the first to touch
   a page of the lazily mapped EABI stack (ARMEABISupport maps the pages one at a time from a data abort handler): 8 or 16 bytes per memory page of the buffer come back
   unwritten (readtest2/3/4).  libunixlib 16.2.0-3 and later touch the pages in user mode first, so the check says "data right" with them.  (Not UnixLib's fread(): read()
   returns the full count.)  Information only, and it has to be the first thing main does, while the stack is fresh.  Result: 1 = data right, 0 = WRONG DATA, -1 = no test.  */
#define FRESH_KB 128
static int fresh_fread_result = -1;

static __attribute__((noinline, optimize("no-stack-clash-protection"))) void fresh_stack_fread(void)
{
  const char *name = "ulinfo_fr";
  size_t len = (size_t) FRESH_KB * 1024;
  unsigned char *pat = malloc(len);
  if (!pat) return;
  for (size_t i = 0; i < len; i++) pat[i] = (unsigned char) (((unsigned) i * 2654435761u) >> 24 ^ (i >> 12));   /* every 4 KB page different */
  FILE *f = fopen(name, "wb");
  if (!f) { free(pat); return; }
  size_t w = fwrite(pat, 1, len, f);
  fclose(f);
  if (w == len) {
    unsigned char buf[FRESH_KB * 1024];           /* deliberately a big array on the stack */
    f = fopen(name, "rb");
    if (f) { size_t r = fread(buf, 1, sizeof buf, f); fclose(f); fresh_fread_result = (r == sizeof buf && !memcmp(buf, pat, sizeof buf)); }
  }
  remove(name);
  free(pat);
}

/* memcpy() of 512 bytes into a stack page that has never been used, starting exactly at a page boundary (so the main loop of the NEON memcpy stores 64-byte aligned lines).  With
   libunixlib before 16.2.0-5 this dies with SIGSEGV on the Cortex-A72 machine this was developed on: a 64-byte aligned vstm that is the first access to a page of the lazily
   mapped EABI stack is not restarted.  The functions of this test (and of the fread test above) are compiled WITHOUT -fstack-clash-protection, the default of the GCC 16 port:
   its probes would map the pages first and the test would always pass.  -1 = could not run, 0 = crashed (caught), 1 = copied correctly.  Information only; it runs first,
   while the stack is fresh.  */
static sigjmp_buf fm_jb;
static void fm_handler(int sig) { (void) sig; siglongjmp(fm_jb, 1); }
static unsigned char fm_src[512];
static int fresh_memcpy_result = -1;

static __attribute__((noinline, optimize("no-stack-clash-protection"))) int fm_copy(void)
{
  volatile unsigned char *region = __builtin_alloca(16384);
  uintptr_t pg = ((uintptr_t) region + 4096 + 4095) & ~(uintptr_t) 4095;       /* a page boundary at least 4 KB into the region */
  unsigned char *dst = (unsigned char *) pg;
  memcpy(dst, fm_src, sizeof fm_src);
  for (size_t i = 0; i < sizeof fm_src; i++) if (dst[i] != fm_src[i]) return 0;
  return 1;
}

static __attribute__((noinline, optimize("no-stack-clash-protection"))) void fresh_memcpy_test(void)
{
  for (size_t i = 0; i < sizeof fm_src; i++) fm_src[i] = (unsigned char) (i * 7 + 3);
  struct sigaction sa, old; memset(&sa, 0, sizeof sa); sa.sa_handler = fm_handler; sigemptyset(&sa.sa_mask);
  sigaction(SIGSEGV, &sa, &old);
  if (sigsetjmp(fm_jb, 1) == 0) {
    volatile unsigned char *skip = __builtin_alloca(300 * 1024);                  /* move well below anything used so far; never touched */
    __asm__ volatile ("" : : "r" (skip) : "memory");
    fresh_memcpy_result = fm_copy();
  } else fresh_memcpy_result = 0;
  sigaction(SIGSEGV, &old, NULL);
}

/* Information only (no checks): is /dev/urandom usable (UnixLib needs the CryptRand module), and what does a clock read cost?  */
static double now_us(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e6 + t.tv_nsec / 1e3; }

static void info_urandom_clocks(void)
{
  int fd = open("/dev/urandom", O_RDONLY);
  if (fd < 0) printf("  INFO /dev/urandom: open failed: %s (errno %d)\n", strerror(errno), errno);
  else {
    unsigned char a[8] = {0}, b[8] = {0}; ssize_t n1 = read(fd, a, 8), n2 = read(fd, b, 8);
    printf("  INFO /dev/urandom: read %ld + %ld bytes, the two blocks are %s: %02x%02x%02x%02x.. / %02x%02x%02x%02x..\n", (long) n1, (long) n2,
           memcmp(a, b, 8) ? "different" : "IDENTICAL", a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3]);
    close(fd);
  }
  struct timespec ts; struct timeval tv; double t0; int n = 200;
  t0 = now_us(); for (int i = 0; i < n; i++) clock_gettime(CLOCK_MONOTONIC, &ts); double mono = (now_us() - t0) / n;
  t0 = now_us(); for (int i = 0; i < n; i++) clock_gettime(CLOCK_REALTIME, &ts);  double real = (now_us() - t0) / n;
  t0 = now_us(); for (int i = 0; i < n; i++) gettimeofday(&tv, NULL);            double gtod = (now_us() - t0) / n;
  t0 = now_us(); for (int i = 0; i < n; i++) (void) time(NULL);                  double tm = (now_us() - t0) / n;
  t0 = now_us(); for (int i = 0; i < n; i++) (void) clock();                     double clk = (now_us() - t0) / n;
  printf("  INFO cost of one call (microseconds, mean of %d, resolution of the timer 10000): CLOCK_MONOTONIC %.1f, CLOCK_REALTIME %.1f, gettimeofday %.1f, time %.1f, clock %.1f\n", n, mono, real, gtod, tm, clk);
}

#include "ul16blocks.h"

/* Which libunixlib FILE is on the disk?  (sysconf above says which one is LOADED; if the two differ an old copy is still in memory.)  Sizes and FNV-1a hashes
   of the libunixlib.so.5.0.0 of every package that was shipped, plus control files that no package ever changed (libgcc_s, libdl, the 10.2.0 libm): if the controls
   match, reading and hashing a file works on this machine and a mismatch in libunixlib is real.  */
struct known { const char *name; unsigned size, fnv; };
static const struct known known_libs[] = {
  { "10.2.0-1 (the original)", 5408732u, 0xddacd5a3u },
  { "10.2.0-2 (relinked by the new ld)", 5408244u, 0xa9adf931u },
  { "10.2.0-3", 5409300u, 0x44e0b812u },
  { "10.2.0-4", 5410148u, 0x07f3374fu },
  { "10.2.0-5", 5412476u, 0x55504e8cu },
  { "16.2.0-1 (built by GCC 16.2)", 5178300u, 0x0191b236u },
  { "16.2.0-2 (built by GCC 16.2, fread/fwrite fix)", 5178368u, 0xe144aeadu },
  { "16.2.0-3 (built by GCC 16.2, touches stack buffers before reads; has no fix-level answer, a packaging error)", 5180816u, 0x6d302c79u },
  { "16.2.0-4 (built by GCC 16.2, touches stack buffers before reads, fix level 6)", 5180912u, 0xab632ed3u },
  { "16.2.0-5 (built by GCC 16.2 with -fstack-clash-protection, memcpy without the 64-byte vstm, fix level 7)", 5195580u, 0x1721db04u },
  { "16.2.0-6 (as 16.2.0-5, plus a configurable main stack (__stack_size) and a heap dynamic area that falls back to a smaller maximum, fix level 8)", 5195844u, 0x19bd576fu },
  { "16.2.0-7 (as 16.2.0-6, plus mmap/mremap refuse a request that can never be served and the signal stack is freed when the process ends, fix level 9)", 5197716u, 0xd18012bbu },
  { "16.2.0-8 (as 16.2.0-7, plus the _exit of a vfork child that ends without exec leaves the RMA block of the shared program image alone, fix level 10)", 5197788u, 0x97df02a7u },
  { "16.2.0-9 (as 16.2.0-8, plus the heap of a vfork + exec child never grows over the copy of its parent: appspace_himem is not raised above the limit the program was started with, fix level 11)", 5198820u, 0xce1ddbc6u },
  { "16.2.0-10 (as 16.2.0-9, plus the inline SWI wrappers no longer read register variables after the asm - with DDEUtils loaded, arguments longer than the program name were cut - and __get_dde_prefix no longer loops for ever when a prefix is set, fix level 12)", 5203436u, 0x11031f6du },
  { "16.2.0-11 (as 16.2.0-10, plus scanf understands ll / q / j / hh / z / t and %Lf stores a long double, fix level 13: the native lto1 could not read the 64 bit id of its section names with sscanf)", 5204904u, 0x0a0cb433u },
};
static const struct known known_libgcc[] = { { "the 10.2.0 libgcc_s.so.1 (unchanged in every package)", 1480628u, 0x4fce8f52u } };
static const struct known known_libdl[] = { { "the 10.2.0 libdl (unchanged in every package)", 22268u, 0xea4d96a4u } };
static const struct known known_libm[] = { { "the 10.2.0 libm", 3212u, 0x05cb92dbu }, { "the GCC 16.2 libm (16.2.0-1)", 2996u, 0xe85270f1u }, { "the GCC 16.2 libm (16.2.0-2)", 2996u, 0xed33c4b2u }, { "the GCC 16.2 libm (16.2.0-3)", 2996u, 0x54ba62bfu }, { "the GCC 16.2 libm (16.2.0-4)", 2996u, 0xa6367883u }, { "the GCC 16.2 libm (16.2.0-5)", 2996u, 0x9e397915u }, { "the GCC 16.2 libm (16.2.0-6)", 2996u, 0x8d511f62u }, { "the GCC 16.2 libm (16.2.0-7)", 2996u, 0x32d714ddu }, { "the GCC 16.2 libm (16.2.0-8)", 2996u, 0xc9b75988u }, { "the GCC 16.2 libm (16.2.0-9)", 2996u, 0xc10a80c6u }, { "the GCC 16.2 libm (16.2.0-10)", 2996u, 0x147a2b80u }, { "the GCC 16.2 libm (16.2.0-11)", 2996u, 0x857f8081u } };

/* Read a whole file in 4096-byte blocks; returns 0 if it cannot be opened.  If it has the size of the 16.2.0-1 libunixlib, also compare every block with that
   library and report how many blocks differ (and the first few).  */
static int hash_file(const char *path, unsigned long *total, unsigned *hash, unsigned *ndiff, unsigned first[], unsigned maxfirst)
{
  FILE *f = fopen(path, "rb");
  if (!f) return 0;
  static unsigned char blk[4096];
  unsigned h = 2166136261u; unsigned long size = 0; unsigned idx = 0, nd = 0;
  for (;;) {
    size_t n = fread(blk, 1, sizeof blk, f), got = n;
    while (got < sizeof blk && n > 0) { n = fread(blk + got, 1, sizeof blk - got, f); got += n; }
    if (got == 0) break;
    unsigned bh = 2166136261u;
    for (size_t k = 0; k < got; k++) { h ^= blk[k]; h *= 16777619u; bh ^= blk[k]; bh *= 16777619u; }
    if (idx < UL16_BLOCKS && bh != ul16_block_fnv[idx]) { if (nd < maxfirst) first[nd] = idx; nd++; }
    size += got; idx++;
    if (got < sizeof blk) break;
  }
  fclose(f);
  *total = size; *hash = h; *ndiff = nd;
  return 1;
}

static void info_one(const char *label, const char *const *names, const struct known *table, unsigned nknown, int blocks)
{
  for (int i = 0; names[i]; i++) {
    unsigned long total; unsigned h, nd; unsigned first[24];
    if (!hash_file(names[i], &total, &h, &nd, first, 24)) continue;
    const char *which = "no file we shipped";
    for (unsigned k = 0; k < nknown; k++) if (table[k].size == total && table[k].fnv == h) which = table[k].name;
    printf("  INFO %s (%s): %lu bytes, FNV %08x = %s\n", label, names[i], total, h, which);
    if (blocks && total == UL16_SIZE && strncmp(which, "16.2.0-1", 8) != 0) {
      printf("       same size as 16.2.0-1, but %u of %d blocks of 4096 bytes differ from it; first differing blocks:", nd, UL16_BLOCKS);
      for (unsigned k = 0; k < nd && k < 24; k++) printf(" %u", first[k]);
      printf("\n");
    }
    return;
  }
  printf("  INFO %s: could not be opened by any of the usual names\n", label);
}

static void info_disk_library(void)
{
  const char *env = getenv("ULINFO_LIB");
  const char *unixlib[] = { env, "SharedLibs:lib.armeabihf.libunixlib/so/5/0/0", "SharedLibs:lib/armeabihf/libunixlib.so.5.0.0", "/<SharedLibs$Dir>/lib/armeabihf/libunixlib.so.5.0.0", NULL };
  const char *libgcc[] = { "SharedLibs:lib.armeabihf.libgcc_s/so/1", "SharedLibs:lib/armeabihf/libgcc_s.so.1", NULL };
  const char *libdl[] = { "SharedLibs:lib.armeabihf.libdl/2/0/0/so", "SharedLibs:lib/armeabihf/libdl.2.0.0.so", NULL };
  const char *libm[] = { "SharedLibs:lib.armeabihf.libm/so/1/0/0", "SharedLibs:lib/armeabihf/libm.so.1.0.0", NULL };
  if (!env) unixlib[0] = unixlib[1], unixlib[1] = unixlib[2], unixlib[2] = unixlib[3], unixlib[3] = NULL;
  info_one("the libunixlib file on the disk", unixlib, known_libs, sizeof known_libs / sizeof known_libs[0], 1);
  info_one("control: libgcc_s", libgcc, known_libgcc, (int) (sizeof known_libgcc / sizeof known_libgcc[0]), 0);
  info_one("control: libdl", libdl, known_libdl, (int) (sizeof known_libdl / sizeof known_libdl[0]), 0);
  info_one("libm", libm, known_libm, (int) (sizeof known_libm / sizeof known_libm[0]), 0);
}

int main(void)
{
  fresh_memcpy_test();
  fresh_stack_fread();
  printf("ulinfo 1.14 [%s]\n", ROTEST_CFG);
  long lvl = sysconf(0x4700), cc = sysconf(0x4701);
  printf("UnixLib fix level (sysconf 0x4700): %ld   (-1: stock 10.2.0-1, -3 or -4, or 16.2.0-3; 5: 10.2.0-5, 16.2.0-1 or 16.2.0-2; 6: 16.2.0-4; 7: 16.2.0-5; 8: 16.2.0-6 = configurable main stack and heap fallback; 9: 16.2.0-7 = also mmap/mremap refuse requests that cannot be served and the signal stack is freed at exit; 10: 16.2.0-8 = also the _exit of a vfork child that ends without exec leaves the RMA block of the shared program image alone; 11: 16.2.0-9 = also the heap of a vfork + exec child never grows over the copy of its parent; 12: 16.2.0-10 or later = also programs started with long arguments work with the DDEUtils module loaded, and a DDEUtils prefix does not hang them; 13: 16.2.0-11 or later = also scanf understands ll, hh, j, z, t and q, and %%Lf stores a long double)\n", lvl);
  printf("libunixlib compiled by GCC (sysconf 0x4701): %ld   (-1: this library does not say)\n", cc);
  printf("sysconf: pagesize %ld, clk_tck %ld, open_max %ld, nprocessors(28) %ld\n", sysconf(_SC_PAGESIZE), sysconf(_SC_CLK_TCK), sysconf(_SC_OPEN_MAX), sysconf(28));
  fflush(stdout);
  test_numbers();   printf("  numbers and libm      %d checks so far\n", checks);
  test_memory_strings(); printf("  memory and strings    %d checks so far\n", checks);
  test_process();   printf("  process, time, threads %d checks so far\n", checks);
  test_files();     printf("  files and directories %d checks so far\n", checks);
  info_urandom_clocks();
  printf("  INFO fread() into a %d KB buffer on a fresh stack: %s\n", FRESH_KB, fresh_fread_result == 1 ? "data right" : fresh_fread_result == 0 ? (lvl >= 6 ? "WRONG DATA although this library (fix level 6+) touches the pages first: see README" : "WRONG DATA (the OS loses a store when it fills stack pages that were never used; libunixlib 16.2.0-3 and later work around it: see README)") : "test could not run");
  printf("  INFO memcpy() of 512 bytes into a fresh stack page at a page boundary: %s\n", fresh_memcpy_result == 1 ? "copied correctly" :
         fresh_memcpy_result == 0 ? "CRASHED (SIGSEGV, caught): the 64-byte vstm bug of libunixlib before 16.2.0-5 (programs built with -fstack-clash-protection, the default of the GCC 16 port, never get there)" : "test could not run");
  info_disk_library();
  printf("SUMMARY [ulinfo]: %d checks, %d failed -> %s\n", checks, fails, fails ? "FAIL" : "PASS");
  return fails != 0;   /* atexit handlers run after this */
}
