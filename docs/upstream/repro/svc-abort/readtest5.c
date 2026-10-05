/* NOTE: the GCC 16 port compiles with -fstack-clash-protection by default, which maps the stack pages before this program reaches them: build it with -fno-stack-clash-protection to see the problem it probes. */
/* readtest5.c (a revised readtest4) -- what exactly goes wrong on a stack page that has never been touched, and why does UnixLib's memcpy crash sometimes?  (readtest3 on the machine: user-mode copies with one kind of store each were right,
   UnixLib's NEON memcpy() into a fresh stack buffer crashed with SIGSEGV, and read() left 16-byte units of the buffer unwritten.)
   usage:  readtest5 fresh KIND                  a table of user-mode copies (64 and 512 bytes, six positions against a page boundary) into really fresh pages
           readtest5 rep KIND DEST SRC N REPS   the SAME copy of N bytes repeated REPS times (DEST stack|static|heap, SRC pat|static): which repetitions crash?
           readtest5 mem KIND BATCH      a table of small copies on fresh stack pages (BATCH 0, 1 or 2 = sizes 1-63, 64-257, 512-4096), one line each: OK / CRASH / BAD
           readtest4 fill KIND KBYTES [PAD]  ONE big fill of a fresh stack buffer by the operating system (or a control), compared with a known pattern
   mem KIND:  memcpy memmove memset | vstm8 vstm4 vst1q vstmdb vlane (user-mode NEON/VFP stores written like UnixLib's memcpy does)  | strd | stm
   fill KIND: read (file) varval (OS_ReadVarVal) gstrans (OS_GSTrans) | memcpy | touch (control: the buffer is written first)
   Every trial uses stack pages that no earlier trial has touched.  A SIGSEGV is caught and the next trial goes on.  Run each command as its own program run.  */
#define _GNU_SOURCE 1
#include <fcntl.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#if defined(__riscos__) || defined(__riscos)
#include <swis.h>
#define HAVE_SWIS 1
#endif

#ifndef ROTEST_CFG
#define ROTEST_CFG "readtest5"
#endif
#define PAGE 4096UL
static const char *path = "SharedLibs:lib.armeabihf.libgcc_s/so/1";

/* ---- a pattern that is different in every byte position and in every run, and never zero ---- */
static unsigned char *pat; static size_t patlen; static unsigned salt;
static unsigned hash32(unsigned x) { x *= 0x9E3779B1u; x ^= x >> 15; x *= 0x85EBCA6Bu; x ^= x >> 13; return x; }
static void make_pattern(size_t n)
{
  static const char alnum[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  patlen = n; pat = malloc(n + 64);
  salt = (unsigned) time(NULL) * 2654435761u ^ (unsigned) clock() ^ ((unsigned) getpid() << 7);
  for (size_t i = 0; i < n + 64; i++) pat[i] = (unsigned char) alnum[hash32((unsigned) i + salt) % 62];
  pat[n + 16] = 0;                       /* a terminator for OS_GSTrans: the pattern itself has none */
}

/* ---- catching SIGSEGV ---- */
static sigjmp_buf jb; static volatile int crashed; static void *volatile crash_addr;
static void on_segv(int sig, siginfo_t *si, void *ctx) { (void) sig; (void) ctx; crash_addr = si ? si->si_addr : NULL; crashed = 1; siglongjmp(jb, 1); }
static void install_handler(void)
{
  struct sigaction sa; memset(&sa, 0, sizeof sa);
#ifdef SA_NODEFER
  sa.sa_flags = SA_SIGINFO | SA_NODEFER;
#else
  sa.sa_flags = SA_SIGINFO;
#endif
  sa.sa_sigaction = on_segv; sigemptyset(&sa.sa_mask);
  sigaction(SIGSEGV, &sa, NULL); sigaction(SIGBUS, &sa, NULL);
#ifdef SIGEMT
  sigaction(SIGEMT, &sa, NULL);
#endif
  sigaction(SIGILL, &sa, NULL);
}

/* ---- user-mode stores written the way UnixLib's memcpy does them (ARM only) ---- */

static inline void tail_bytes(unsigned char *d, const unsigned char *s, size_t n) { for (; n; n--) *(volatile unsigned char *) d++ = *s++; }
#if defined(__arm__)
#define NEONF __attribute__((noinline, target("fpu=neon")))
static NEONF void k_vstm8(unsigned char *d, const unsigned char *s, size_t n)  /* 64-byte vldmia/vstmia of d0-d7 */
{ for (; n >= 64; n -= 64) __asm__ volatile ("vldmia %1!, {d0-d7}\n\tvstmia %0!, {d0-d7}" : "+r" (d), "+r" (s) : : "d0","d1","d2","d3","d4","d5","d6","d7","memory"); tail_bytes(d, s, n); }
static NEONF void k_vstm4(unsigned char *d, const unsigned char *s, size_t n)  /* 32-byte, d0-d3 */
{ for (; n >= 32; n -= 32) __asm__ volatile ("vldmia %1!, {d0-d3}\n\tvstmia %0!, {d0-d3}" : "+r" (d), "+r" (s) : : "d0","d1","d2","d3","memory"); tail_bytes(d, s, n); }
static NEONF void k_vst1q(unsigned char *d, const unsigned char *s, size_t n)  /* NEON vld1/vst1 of 4 d registers */
{ for (; n >= 32; n -= 32) __asm__ volatile ("vld1.8 {d0-d3}, [%1]!\n\tvst1.8 {d0-d3}, [%0]!" : "+r" (d), "+r" (s) : : "d0","d1","d2","d3","memory"); tail_bytes(d, s, n); }
static NEONF void k_vlane(unsigned char *d, const unsigned char *s, size_t n)  /* single-lane byte stores, as memcpy's leading bytes */
{ for (; n; n--) __asm__ volatile ("vld1.8 {d7[7]}, [%1]!\n\tvst1.8 {d7[7]}, [%0]!" : "+r" (d), "+r" (s) : : "d7","memory"); }
static NEONF void k_vstmdb(unsigned char *d, const unsigned char *s, size_t n) /* pre-decrement store with writeback, d8-d9 like vpush; copies backwards 16 bytes at a time */
{ unsigned char *de = d + n; const unsigned char *se = s + n; for (; n >= 16; n -= 16) __asm__ volatile ("vldmdb %1!, {d8-d9}\n\tvstmdb %0!, {d8-d9}" : "+r" (de), "+r" (se) : : "d8","d9","memory"); tail_bytes(d, s, n); }
static void k_strd(unsigned char *d, const unsigned char *s, size_t n)
{ for (; n >= 8; n -= 8) __asm__ volatile ("ldrd r2, r3, [%1], #8\n\tstrd r2, r3, [%0], #8" : "+r" (d), "+r" (s) : : "r2","r3","memory"); tail_bytes(d, s, n); }
static void k_stm(unsigned char *d, const unsigned char *s, size_t n)
{ for (; n >= 32; n -= 32) __asm__ volatile ("ldmia %1!, {r2-r9}\n\tstmia %0!, {r2-r9}" : "+r" (d), "+r" (s) : : "r2","r3","r4","r5","r6","r7","r8","r9","memory"); tail_bytes(d, s, n); }
#define HAVE_KERNELS 1
#endif

static int run_kind(const char *kind, unsigned char *d, const unsigned char *s, size_t n)   /* returns 0 if the kind is unknown */
{
  if (!strcmp(kind, "memcpy")) { memcpy(d, s, n); return 1; }
  if (!strcmp(kind, "memmove")) { memmove(d, s, n); return 1; }
  if (!strcmp(kind, "memset")) { memset(d, s[0], n); return 1; }
#ifdef HAVE_KERNELS
  if (!strcmp(kind, "vstm8")) { k_vstm8(d, s, n); return 1; }
  if (!strcmp(kind, "vstm4")) { k_vstm4(d, s, n); return 1; }
  if (!strcmp(kind, "vst1q")) { k_vst1q(d, s, n); return 1; }
  if (!strcmp(kind, "vlane")) { k_vlane(d, s, n); return 1; }
  if (!strcmp(kind, "vstmdb")) { k_vstmdb(d, s, n); return 1; }
  if (!strcmp(kind, "strd")) { k_strd(d, s, n); return 1; }
  if (!strcmp(kind, "stm")) { k_stm(d, s, n); return 1; }
#endif
  return 0;
}

/* ---- table of small copies ---- */
struct res { int status; size_t bad, first; unsigned long dst; int known; };      /* 0 ok, 1 bad data, 2 crash */
static struct res g_res;
static const unsigned char *g_src;                       /* what the trials copy from (default: the pattern, 8 bytes in) */
#define REGION 12352UL
#define STEP   (REGION + 4096UL)

static __attribute__((noinline)) void mem_trial(const char *kind, size_t n, int variant, size_t unalign)
{
  volatile unsigned char *region = __builtin_alloca(REGION);       /* fresh pages, nobody has touched them */
  uintptr_t P = ((uintptr_t) region + 4096 + PAGE - 1) & ~(PAGE - 1);   /* a memory page boundary inside the region */
  uintptr_t d = variant == 0 ? P - ((n / 2) & ~15UL) : variant == 1 ? P : P - n;
  unsigned char *dst = (unsigned char *) (d + unalign);
  g_res.dst = (unsigned long) dst; g_res.known = 1;
  g_res.known = run_kind(kind, dst, g_src, n);
  size_t bad = 0, first = (size_t) -1;
  if (strcmp(kind, "memset") == 0) { for (size_t i = 0; i < n; i++) if (dst[i] != g_src[0]) { bad++; if (first == (size_t) -1) first = i; } }
  else { for (size_t i = 0; i < n; i++) if (dst[i] != g_src[i]) { bad++; if (first == (size_t) -1) first = i; } }
  g_res.bad = bad; g_res.first = first; g_res.status = bad ? 1 : 0;
}
static __attribute__((noinline)) void mem_at_depth(size_t depth, const char *kind, size_t n, int variant, size_t unalign)
{
  volatile unsigned char *skip = __builtin_alloca(depth);          /* move down to pages no earlier trial reached; never touched */
  __asm__ volatile ("" : : "r" (skip) : "memory");                  /* without this the compiler drops the alloca and every trial reuses the same pages */
  mem_trial(kind, n, variant, unalign);
}

/* ---- user-mode copies into pages that are really fresh: every trial is STEP bytes deeper than the one before ---- */
static __attribute__((noinline)) void fresh_trial(const char *kind, size_t n, long off)
{
  volatile unsigned char *region = __builtin_alloca(REGION);
  uintptr_t P = ((uintptr_t) region + 4096 + PAGE - 1) & ~(PAGE - 1);            /* a page boundary inside the region */
  unsigned char *dst = (unsigned char *) (P + off);
  g_res.dst = (unsigned long) dst;
  g_res.known = run_kind(kind, dst, g_src, n);
  size_t bad = 0, first = (size_t) -1;
  if (!strcmp(kind, "memset")) { for (size_t i = 0; i < n; i++) if (dst[i] != g_src[0]) { bad++; if (first == (size_t) -1) first = i; } }
  else { for (size_t i = 0; i < n; i++) if (dst[i] != g_src[i]) { bad++; if (first == (size_t) -1) first = i; } }
  g_res.bad = bad; g_res.first = first; g_res.status = bad ? 1 : 0;
}
static __attribute__((noinline)) void fresh_at_depth(size_t depth, const char *kind, size_t n, long off)
{
  volatile unsigned char *skip = __builtin_alloca(depth);
  __asm__ volatile ("" : : "r" (skip) : "memory");
  fresh_trial(kind, n, off);
}

static int cmd_fresh(const char *kind)
{
  static const size_t sizes[2] = { 64, 512 };
  static const long offs[6] = { 0, -8, -16, -32, -48, -64 };   /* where the copy starts relative to a page boundary: 0 = exactly at it, -32 = in the middle of a 64-byte block before it, ... */
  printf("  (each line: one user-mode copy into two pages nobody has touched; the next line uses pages deeper in the stack)\n");
  size_t depth = 0;
  for (int i = 0; i < 2; i++)
    for (int j = 0; j < 6; j++) {
      memset(&g_res, 0, sizeof g_res); crashed = 0; crash_addr = 0;
      install_handler();
      if (sigsetjmp(jb, 1) == 0) fresh_at_depth(depth, kind, sizes[i], offs[j]);
      else g_res.status = 2;
      depth += STEP;
      if (g_res.status != 2 && !g_res.known) { printf("  unknown kind %s\n", kind); return 2; }
      printf("  n=%-4zu copy starts %3ld bytes from the page boundary (dst page offset %#5lx): ", sizes[i], offs[j], (unsigned long) (g_res.dst & 4095));
      if (g_res.status == 0) printf("OK\n");
      else if (g_res.status == 1) printf("BAD DATA %zu bytes, first at +%zu\n", g_res.bad, g_res.first);
      else printf("CRASH (SIGSEGV)\n");
    }
  return 0;
}

static int allows_misalign(const char *kind)
{
  return !strcmp(kind, "memcpy") || !strcmp(kind, "memmove") || !strcmp(kind, "memset") || !strcmp(kind, "vst1q") || !strcmp(kind, "vlane");
}

static int cmd_mem(const char *kind, int batch)
{
  static const size_t sizes[3][12] = { { 1, 7, 15, 16, 17, 31, 32, 33, 47, 48, 63, 0 },
                                       { 64, 65, 96, 100, 127, 128, 129, 192, 255, 256, 257, 0 },
                                       { 512, 513, 1000, 1024, 2048, 3000, 4000, 4096, 0, 0, 0, 0 } };
  static const char *vname[3] = { "straddles a page boundary", "starts at a page boundary", "ends at a page boundary" };
  if (batch < 0 || batch > 2) { printf("batch must be 0, 1 or 2\n"); return 2; }
  printf("  (each line: one copy into pages that were never used; the next line uses pages deeper in the stack)\n");
  size_t depth = 0;
  for (int i = 0; i < 12 && sizes[batch][i]; i++) {
    size_t n = sizes[batch][i];
    for (int t = 0; t < 5; t++) {
      int v = t < 3 ? t : t - 3;                       /* variants 0, 1, 2 aligned, then 0 and 1 again misaligned by 3 */
      size_t un = t < 3 ? 0 : 3;
      if (un && !allows_misalign(kind)) continue;       /* multi-register stores need word alignment: a misaligned one is an alignment fault, not a finding */
      memset(&g_res, 0, sizeof g_res); crashed = 0; crash_addr = 0;
      install_handler();
      if (sigsetjmp(jb, 1) == 0) mem_at_depth(depth, kind, n, v, un);
      else g_res.status = 2;
      depth += STEP;
      if (g_res.status != 2 && !g_res.known) { printf("  unknown kind %s\n", kind); return 2; }
      printf("  n=%-5zu %-26s page offset %#5lx%s: ", n, vname[v], (unsigned long) (g_res.dst & 4095), un ? " (+3)" : "     ");
      if (g_res.status == 0) printf("OK\n");
      else if (g_res.status == 1) printf("BAD DATA %zu bytes, first at +%zu\n", g_res.bad, g_res.first);
      else printf("CRASH (SIGSEGV%s%p)\n", crash_addr ? ", address " : "", (void *) crash_addr);
    }
  }
  return 0;
}

/* ---- the same copy again and again ---- */
static unsigned char sbuf_dst[32768] __attribute__((aligned(64)));
static unsigned char sbuf_src[32768] __attribute__((aligned(64)));

static int cmd_rep(const char *kind, const char *dest, const char *srcname, size_t n, int reps)
{
  if (n > 16384) { printf("n at most 16384\n"); return 2; }
  if (!strcmp(srcname, "static")) { for (size_t i = 0; i < sizeof sbuf_src; i++) ((volatile unsigned char *) sbuf_src)[i] = pat[8 + i]; g_src = sbuf_src; }
  else g_src = pat + 8;
  unsigned char *heapbuf = malloc(32768 + 64);
  size_t depth = 0; int ok = 0, bad = 0, crash = 0;
  printf("  %s of %zu bytes, destination %s, source %s (%p), %d repetitions\n", kind, n, dest, srcname, (const void *) g_src, reps);
  for (int i = 0; i < reps; i++) {
    memset(&g_res, 0, sizeof g_res); crashed = 0; crash_addr = 0;
    install_handler();
    if (sigsetjmp(jb, 1) == 0) {
      if (!strcmp(dest, "stack")) fresh_at_depth(depth, kind, n, 0);                   /* a fresh region each time, the copy starts at a page boundary */
      else {
        unsigned char *dst = !strcmp(dest, "static") ? sbuf_dst : heapbuf;
        g_res.dst = (unsigned long) dst;
        g_res.known = run_kind(kind, dst, g_src, n);
        size_t b = 0; for (size_t k = 0; k < n; k++) if (dst[k] != g_src[k]) b++;
        g_res.bad = b; g_res.status = b ? 1 : 0;
      }
    } else g_res.status = 2;
    depth += STEP;
    if (g_res.status == 0) ok++; else if (g_res.status == 1) bad++; else crash++;
    printf("  repetition %2d: dst %#lx: %s\n", i + 1, g_res.dst, g_res.status == 0 ? "OK" : g_res.status == 1 ? "BAD DATA" : "CRASH");
  }
  printf("  summary: %d OK, %d BAD DATA, %d CRASH\n", ok, bad, crash);
  return 0;
}

/* ---- one big fill ---- */
static unsigned char *file_data; static long file_size;
static void load_file(void)
{
  int fd = open(path, O_RDONLY); if (fd < 0) { printf("cannot open %s\n", path); exit(2); }
  file_size = lseek(fd, 0, SEEK_END); lseek(fd, 0, SEEK_SET);
  file_data = malloc(file_size); long got = 0; ssize_t k; while (got < file_size && (k = read(fd, file_data + got, file_size - got)) > 0) got += k;
  close(fd); if (got != file_size) { printf("short read of the reference copy\n"); exit(2); }
}

struct diff { size_t off; unsigned char got, want; };
static struct diff diffs[50000];
static void analyse(const unsigned char *buf, const unsigned char *want, size_t len, long returned, const char *what)
{
  size_t n = 0, total = 0;
  for (size_t i = 0; i < len; i++) if (buf[i] != want[i]) { if (n < 50000) { diffs[n].off = i; diffs[n].got = buf[i]; diffs[n].want = want[i]; n++; } total++; }
  printf("  %s: returned %ld of %zu bytes; %zu bytes differ from what it should be\n", what, returned, len, total);
  if (!total) { printf("  data: every byte right\n"); return; }
  unsigned long start = (unsigned long) buf;
  /* runs of wrong bytes, merged into units: show the units (address, length) */
  printf("  damaged units (address, in-page offset, length of the run of wrong bytes) - first 40:\n   ");
  size_t units = 0, k = 0;
  while (k < n) {
    size_t j = k; while (j + 1 < n && diffs[j + 1].off - diffs[j].off <= 16 && (diffs[j + 1].off >> 4) - (diffs[k].off >> 4) <= 1) j++;
    unsigned long a = start + diffs[k].off;
    if (units < 40) printf(" %lx(+%03lx,%zu)", a, a & 4095, diffs[j].off - diffs[k].off + 1);
    if (units < 40 && units % 4 == 3) printf("\n   ");
    units++; k = j + 1;
  }
  printf("\n  %zu damaged units in all\n", units);
  unsigned long hist[PAGE / 16] = {0}; unsigned long zeros = 0;
  for (size_t q = 0; q < n; q++) { hist[((start + diffs[q].off) & 4095) / 16]++; if (!diffs[q].got) zeros++; }
  printf("  by position in the memory page (16-byte buckets, offset: bytes):");
  for (size_t b = 0; b < PAGE / 16; b++) if (hist[b]) printf(" %03zx:%lu", b * 16, hist[b]);
  printf("\n  of the wrong bytes %lu are 0x00 (the rest is stale page contents)\n", zeros);
  size_t shown = 0; unsigned long lastb = ~0ul;
  for (size_t q = 0; q < n && shown < 3; q++) {
    unsigned long a = start + diffs[q].off, b = (a + PAGE - 1) & ~(PAGE - 1);
    if (b == lastb || b - 16 < start || b + 16 > start + len) continue;
    lastb = b; shown++; size_t o = b - start;
    printf("  around the page boundary %lx: got ", b); for (int i = -16; i < 16; i++) printf("%02x%s", buf[o + i], i == -1 ? "|" : "");
    printf("\n                            want "); for (int i = -16; i < 16; i++) printf("%02x%s", want[o + i], i == -1 ? "|" : ""); printf("\n");
  }
}

static __attribute__((noinline)) void fill_trial(const char *kind, size_t len, size_t pad)
{
  size_t total = len + 4 * PAGE;
  unsigned char *base = __builtin_alloca(total);
  unsigned char *buf = base + 2 * PAGE - (pad % PAGE);
  int local; void *addr = 0; size_t size = 0, guard = 0; pthread_attr_t a;
  printf("  buffer %p..%p (start %lu bytes into its memory page), a local at %p", (void *) buf, (void *) (buf + len), (unsigned long) ((unsigned long) buf & 4095), (void *) &local);
  if (!pthread_getattr_np(pthread_self(), &a)) { pthread_attr_getstack(&a, &addr, &size); pthread_attr_getguardsize(&a, &guard); printf("; stack %p..%p guard %zu", addr, (char *) addr + size, guard); }
  printf("\n");
  const unsigned char *want = pat; long got = 0; const char *what = kind;
  if (!strcmp(kind, "touch")) { memset(buf, 0x5a, len); }
  if (!strcmp(kind, "read") || !strcmp(kind, "touch")) {
    int fd = open(path, O_RDONLY); if (fd < 0) { printf("cannot open\n"); exit(2); }
    while ((size_t) got < len) { ssize_t n = read(fd, buf + got, len - got); if (n <= 0) break; got += n; }
    close(fd); want = file_data;
  }
  else if (!strcmp(kind, "memcpy")) { memcpy(buf, pat, len); got = (long) len; }
#ifdef HAVE_SWIS
  else if (!strcmp(kind, "gstrans")) {
    int outlen = 0;
    char *src = malloc(len + 1); memcpy(src, pat, len); src[len] = 0;          /* a string of exactly len characters */
    _kernel_oserror *e = _swix(OS_GSTrans, _INR(0, 2) | _OUT(2), src, buf, (int) len + 16, &outlen);
    if (e) { printf("  OS_GSTrans failed: %s\n", e->errmess); return; }
    got = outlen;
  }
#endif
  else { printf("  kind %s is not available here\n", kind); return; }
  if ((size_t) got < len) printf("  (the call delivered only %ld of %zu bytes)\n", got, len);
  analyse(buf, want, len < (size_t) got ? len : (size_t) got, got, what);
}

static int cmd_fill(const char *kind, size_t kb, size_t pad)
{
  size_t len = kb * 1024;
  if (!strcmp(kind, "read") || !strcmp(kind, "touch")) { load_file(); if (len > (size_t) file_size) { printf("size beyond the file\n"); return 2; } }
  install_handler();
  if (sigsetjmp(jb, 1) == 0) fill_trial(kind, len, pad);
  else printf("  CRASH (SIGSEGV%s%p)\n", crash_addr ? ", address " : "", (void *) crash_addr);
  return 0;
}

int main(int argc, char **argv)
{
  if (argc < 3 || (argc < 4 && strcmp(argv[1], "fresh"))) { printf("usage: readtest5 fresh KIND | readtest5 rep KIND DEST SRC N REPS | readtest5 mem KIND BATCH | readtest5 fill KIND KBYTES [PAD]\n"); return 2; }
  printf("readtest5 1.0 [%s]", ROTEST_CFG); for (int i = 1; i < argc; i++) printf(" %s", argv[i]); printf("\n");
  make_pattern(300 * 1024);
  g_src = pat + 8;
  if (!strcmp(argv[1], "rep")) { if (argc < 7) { printf("usage: readtest5 rep KIND DEST SRC N REPS\n"); return 2; } return cmd_rep(argv[2], argv[3], argv[4], (size_t) atoi(argv[5]), atoi(argv[6])); }
  if (!strcmp(argv[1], "fresh")) return cmd_fresh(argv[2]);
  if (!strcmp(argv[1], "mem")) return cmd_mem(argv[2], atoi(argv[3]));
  if (!strcmp(argv[1], "fill")) return cmd_fill(argv[2], (size_t) atoi(argv[3]), argc > 4 ? (size_t) atoi(argv[4]) : 0);
  printf("unknown command\n"); return 2;
}
