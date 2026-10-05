/* NOTE: the GCC 16 port compiles with -fstack-clash-protection by default, which maps the stack pages before this program reaches them: build it with -fno-stack-clash-protection to see the problem it probes. */
/* readtest3.c -- WHICH bytes go wrong when read() fills stack memory the program has never touched, and when.
   (readtest2 on the machine: read() returns the full count, no short reads, but a few bytes per 4 KB page come out wrong, the first 8 bytes before a memory page
   boundary; the same read into memory that was written first, into mmap or malloc memory, or into a static buffer is right.)
   usage: readtest3 [-f FILE] [-p PAD] [-c CHUNK] [-s SKIP] MODE KBYTES
     -f FILE   file to read (default SharedLibs:lib.armeabihf.libgcc_s/so/1)
     -p PAD    start the buffer PAD bytes later (moves the memory page boundaries relative to the file offsets)
     -c CHUNK  read in pieces of CHUNK bytes instead of one read()
     -s SKIP   start reading at file offset SKIP
   MODE  read   read() into a fresh stack buffer           memcpy  control: CPU copy (UnixLib memcpy) of the reference data into a fresh stack buffer
         cpu-str cpu-strb cpu-strd cpu-ldm cpu-vstr cpu-vstm   the same copy by the CPU in user mode with ONE kind of store instruction
         urandom  read() from /dev/urandom (a module fills the buffer in supervisor mode); counts zeroed 8-byte windows before page boundaries
         model  (anywhere) a simulation of the damage the first run showed, to see what this report looks like
         upper  write the upper half of the buffer first (those pages become mapped), then read()      
         top    write only the TOP page of the buffer first, then read()
   Run each case in its own process (fresh stack).  The output lists the differing bytes (offset, address, what came out, what the file has) and, per memory page,
   where in the page they are.  */
#define _GNU_SOURCE 1
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef ROTEST_CFG
#define ROTEST_CFG "readtest3"
#endif
#define PAGE 4096
static const char *path = "SharedLibs:lib.armeabihf.libgcc_s/so/1";
static unsigned char *truth; static long truth_size;
static size_t pad, chunk, skip;

static void load_truth(void)
{
  int fd = open(path, O_RDONLY);
  if (fd < 0) { printf("cannot open %s\n", path); exit(2); }
  off_t size = lseek(fd, 0, SEEK_END); lseek(fd, 0, SEEK_SET);
  truth = malloc(size); long got = 0; ssize_t n;
  while (got < size && (n = read(fd, truth + got, size - got)) > 0) got += n;
  close(fd);
  if (got != size) { printf("short read of the reference copy\n"); exit(2); }
  truth_size = got;
}

struct diff { size_t off; unsigned char got, want; };
static struct diff diffs[100000]; static size_t ndiffs, total_diffs;

static void analyse(const unsigned char *buf, size_t len, long returned)
{
  const unsigned char *want = truth + skip;
  size_t n = 0;
  for (size_t i = 0; i < len; i++) if (buf[i] != want[i]) { if (n < sizeof diffs / sizeof diffs[0]) { diffs[n].off = i; diffs[n].got = buf[i]; diffs[n].want = want[i]; n++; } total_diffs++; }
  ndiffs = n;
  printf("  returned %ld of %zu bytes; %zu bytes differ from the file\n", returned, len, total_diffs);
  if (!total_diffs) { printf("  data: every byte right\n"); return; }
  unsigned long start = (unsigned long) buf;
  /* per memory page */
  printf("  per memory page (address-aligned 4 KB): page address: differing bytes, in-page offsets min..max\n");
  size_t shown = 0; unsigned long curpage = ~0ul; unsigned cnt = 0, mn = 0, mx = 0;
  unsigned long hist_mem[PAGE / 8] = {0}, hist_file[PAGE / 8] = {0}; unsigned long zeros = 0;
  for (size_t k = 0; k <= ndiffs; k++) {
    unsigned long page = k < ndiffs ? (start + diffs[k].off) & ~(unsigned long) (PAGE - 1) : ~0ul - 1;
    if (page != curpage) {
      if (cnt && shown < 40) { printf("    %08lx: %3u bytes, in-page 0x%03x..0x%03x\n", curpage, cnt, mn, mx); shown++; }
      curpage = page; cnt = 0;
    }
    if (k == ndiffs) break;
    unsigned inpage = (unsigned) ((start + diffs[k].off) & (PAGE - 1));
    if (!cnt) mn = mx = inpage;
    if (inpage < mn) mn = inpage;
    if (inpage > mx) mx = inpage;
    cnt++;
    hist_mem[inpage / 8]++; hist_file[((skip + diffs[k].off) & (PAGE - 1)) / 8]++;
    if (!diffs[k].got) zeros++;
  }
  printf("  differing bytes that came out as 0x00: %lu of %zu\n", zeros, ndiffs);
  printf("  histogram by position in the memory page (8-byte buckets, in-page offset: count):");
  for (int b = 0; b < PAGE / 8; b++) if (hist_mem[b]) printf(" %03x:%lu", b * 8, hist_mem[b]);
  printf("\n  histogram by position in the file page ((file offset) mod 4096, 8-byte buckets):");
  for (int b = 0; b < PAGE / 8; b++) if (hist_file[b]) printf(" %03x:%lu", b * 8, hist_file[b]);
  printf("\n");
  /* the first differing bytes in detail */
  printf("  first differing bytes (buffer offset, address, got, file):\n   ");
  for (size_t k = 0; k < ndiffs && k < 48; k++) { printf(" %zu@%lx %02x/%02x", diffs[k].off, start + diffs[k].off, diffs[k].got, diffs[k].want); if (k % 6 == 5) printf("\n   "); }
  printf("\n");
  /* where do the wrong 8 bytes come from? look for them in the file data around (at 4-byte steps, +-256 bytes) */
  printf("  the 8 wrong bytes before the first boundaries: do they occur nearby in the file data?\n");
  {
    size_t seen2 = 0; unsigned long lastb2 = ~0ul;
    for (size_t k = 0; k < ndiffs && seen2 < 8; k++) {
      unsigned long a = start + diffs[k].off, b = (a + PAGE - 1) & ~(unsigned long) (PAGE - 1);
      if (b == lastb2) continue;
      lastb2 = b; seen2++;
      if (b - 8 < start || b > start + len) continue;
      size_t o = b - start - 8; long found = 0; int any = 0;
      for (long d = -256; d <= 256; d += 4) {
        if (d == 0 || (long) o + d < 0 || o + d + 8 > len + skip + 0 || o + skip + d + 8 > (size_t) truth_size) continue;
        if (!memcmp(buf + o, truth + skip + o + d, 8)) { found = d; any = 1; break; }
      }
      int allzero = 1; for (int q = 0; q < 8; q++) if (buf[o + q]) allzero = 0;
      printf("    boundary %lx: got ", b); for (int q = 0; q < 8; q++) printf("%02x", buf[o + q]);
      printf(" file "); for (int q = 0; q < 8; q++) printf("%02x", truth[skip + o + q]);
      if (allzero) printf("  -> all zero (the store never happened?)\n");
      else if (any) printf("  -> the same 8 bytes occur in the file %+ld bytes away (an old/new value stored again?)\n", found);
      else printf("  -> not found nearby\n");
    }
  }
  /* the 16 bytes around the first few corrupted page boundaries */
  size_t seen = 0; unsigned long lastb = ~0ul;
  for (size_t k = 0; k < ndiffs && seen < 4; k++) {
    unsigned long a = start + diffs[k].off, b = (a + PAGE - 1) & ~(unsigned long) (PAGE - 1);
    if (b == lastb) continue;
    lastb = b; seen++;
    if (b - 16 < start || b + 16 > start + len) continue;
    size_t o = b - start;
    printf("  around the boundary at %lx (buffer offset %zu): got ", b, o);
    for (int i = -16; i < 16; i++) printf("%02x%s", buf[o + i], i == -1 ? "|" : "");
    printf("\n                                    file ");
    for (int i = -16; i < 16; i++) printf("%02x%s", want[o + i], i == -1 ? "|" : "");
    printf("\n");
  }
}

/* Copies done by the CPU in user mode with ONE kind of store instruction each, to see whether the instruction that faults on a page that is not mapped yet is restarted
   properly.  ARM only; elsewhere they fall back to memcpy.  n is a multiple of 32, pointers are 8-byte aligned.  */
#if defined(__arm__)
static void cp_str(unsigned char *d, const unsigned char *s, size_t n)
{ for (; n >= 4; n -= 4, d += 4, s += 4) *(volatile unsigned *) d = *(const volatile unsigned *) s; }
static void cp_strb(unsigned char *d, const unsigned char *s, size_t n)
{ for (; n; n--, d++, s++) *(volatile unsigned char *) d = *(const volatile unsigned char *) s; }
static void cp_strd(unsigned char *d, const unsigned char *s, size_t n)
{ for (; n >= 8; n -= 8) __asm__ volatile ("ldrd r2, r3, [%1], #8\n\tstrd r2, r3, [%0], #8" : "+r" (d), "+r" (s) : : "r2", "r3", "memory"); }
static void cp_ldm(unsigned char *d, const unsigned char *s, size_t n)
{ for (; n >= 32; n -= 32) __asm__ volatile ("ldmia %1!, {r2-r9}\n\tstmia %0!, {r2-r9}" : "+r" (d), "+r" (s) : : "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "memory"); }
static void cp_vstr(unsigned char *d, const unsigned char *s, size_t n)
{ for (; n >= 8; n -= 8, d += 8, s += 8) __asm__ volatile ("vldr d0, [%1]\n\tvstr d0, [%0]" : : "r" (d), "r" (s) : "d0", "memory"); }
static void cp_vstm(unsigned char *d, const unsigned char *s, size_t n)
{ for (; n >= 32; n -= 32) __asm__ volatile ("vldmia %1!, {d0-d3}\n\tvstmia %0!, {d0-d3}" : "+r" (d), "+r" (s) : : "d0", "d1", "d2", "d3", "memory"); }
#define HAVE_CPU_COPIES 1
#endif

static int cpu_copy(const char *mode, unsigned char *d, const unsigned char *s, size_t n)
{
#ifdef HAVE_CPU_COPIES
  if (!strcmp(mode, "cpu-str"))  { cp_str(d, s, n);  return 1; }
  if (!strcmp(mode, "cpu-strb")) { cp_strb(d, s, n); return 1; }
  if (!strcmp(mode, "cpu-strd")) { cp_strd(d, s, n); return 1; }
  if (!strcmp(mode, "cpu-ldm"))  { cp_ldm(d, s, n);  return 1; }
  if (!strcmp(mode, "cpu-vstr")) { cp_vstr(d, s, n); return 1; }
  if (!strcmp(mode, "cpu-vstm")) { cp_vstm(d, s, n); return 1; }
#endif
  if (!strncmp(mode, "cpu-", 4)) { printf("  (%s is not available here: plain memcpy)\n", mode); memcpy(d, s, n); return 1; }
  return 0;
}

static __attribute__((noinline)) void run(const char *mode, size_t len)
{
  size_t total = len + pad + 2 * PAGE;
  unsigned char *base = __builtin_alloca(total);
  unsigned char *buf = base + pad;
  int local; void *addr = 0; size_t size = 0, guard = 0; pthread_attr_t a;
  printf("  buffer %p..%p, a local at %p", (void *) buf, (void *) (buf + len), (void *) &local);
  if (!pthread_getattr_np(pthread_self(), &a)) { pthread_attr_getstack(&a, &addr, &size); pthread_attr_getguardsize(&a, &guard); printf("; stack %p..%p guard %zu", addr, (char *) addr + size, guard); }
  printf("\n");
  if (!strcmp(mode, "upper")) memset(buf + len / 2, 0x5a, len - len / 2);
  if (!strcmp(mode, "top")) memset(buf + len - PAGE, 0x5a, PAGE);
  long got = 0;
  if (!strcmp(mode, "memcpy")) { memcpy(buf, truth + skip, len); got = (long) len; }
  else if (!strncmp(mode, "cpu-", 4)) { cpu_copy(mode, buf, truth + skip, len); got = (long) len; }
  else if (!strcmp(mode, "urandom")) {   /* a module (CryptRand) fills the buffer in supervisor mode: count the page boundaries whose last 8 bytes are all zero */
    int fd = open("/dev/urandom", O_RDONLY); if (fd < 0) { printf("  cannot open /dev/urandom\n"); exit(2); }
    size_t g = 0; ssize_t n; while (g < len && (n = read(fd, buf + g, len - g)) > 0) g += n; close(fd);
    unsigned bounds = 0, zero8 = 0, zero_in_page_end = 0;
    for (unsigned long b = ((unsigned long) buf + PAGE - 1) & ~(unsigned long) (PAGE - 1); b + 8 <= (unsigned long) buf + len; b += PAGE) {
      if (b - 8 < (unsigned long) buf) continue;
      bounds++; const unsigned char *w = (const unsigned char *) (b - 8); int z = 1; for (int k = 0; k < 8; k++) if (w[k]) z = 0; if (z) zero8++;
      int nz = 0; for (int k = 0; k < 8; k++) if (!w[k]) nz++; if (nz >= 2) zero_in_page_end++;
    }
    printf("  urandom: read %zu bytes; of %u memory page boundaries inside the buffer, %u have all of the 8 bytes before them zero, %u have two or more zero bytes there (random data: expect 0 and about %u)\n", g, bounds, zero8, zero_in_page_end, bounds / 36);
    return;
  }
  else if (!strcmp(mode, "model")) {      /* a simulation, to see what the report looks like: the 8 bytes before every memory page boundary (but the first) are zero */
    memcpy(buf, truth + skip, len); got = (long) len;
    for (unsigned long b = ((unsigned long) buf + PAGE - 1) & ~(unsigned long) (PAGE - 1); b + PAGE <= (unsigned long) buf + len + 0; b += PAGE) if (b > (unsigned long) buf + PAGE) memset((void *) (b - 8), 0, 8);
  }
  else {
    int fd = open(path, O_RDONLY); if (fd < 0) { printf("cannot open\n"); exit(2); }
    if (skip) lseek(fd, (off_t) skip, SEEK_SET);
    size_t piece = chunk ? chunk : len; int calls = 0;
    while ((size_t) got < len) {
      size_t want = len - got < piece ? len - got : piece;
      ssize_t n = read(fd, buf + got, want);
      calls++;
      if (n <= 0) break;
      got += n;
    }
    close(fd);
    printf("  %d read() call(s)\n", calls);
  }
  analyse(buf, len, got);
}

int main(int argc, char **argv)
{
  int i = 1;
  for (; i < argc && argv[i][0] == '-' && argv[i][1]; i += 2) {
    if (i + 1 >= argc) break;
    if (!strcmp(argv[i], "-f")) path = argv[i + 1];
    else if (!strcmp(argv[i], "-p")) pad = (size_t) atol(argv[i + 1]);
    else if (!strcmp(argv[i], "-c")) chunk = (size_t) atol(argv[i + 1]);
    else if (!strcmp(argv[i], "-s")) skip = (size_t) atol(argv[i + 1]);
    else { printf("unknown option %s\n", argv[i]); return 2; }
  }
  if (argc - i < 2) { printf("usage: readtest3 [-f FILE] [-p PAD] [-c CHUNK] [-s SKIP] read|memcpy|upper|top KBYTES\n"); return 2; }
  const char *mode = argv[i]; size_t len = (size_t) atoi(argv[i + 1]) * 1024;
  printf("readtest3 1.0 [%s] %s %s KB, file %s, pad %zu, chunk %zu, skip %zu\n", ROTEST_CFG, mode, argv[i + 1], path, pad, chunk, skip);
  load_truth();
  if (skip + len > (size_t) truth_size) { printf("skip + size is beyond the end of the file (%ld bytes)\n", truth_size); return 2; }
  run(mode, len);
  return 0;
}
