/* NOTE: the GCC 16 port compiles with -fstack-clash-protection by default, which maps the stack pages before this program reaches them: build it with -fno-stack-clash-protection to see the problem it probes. */
/* readtest2.c -- what does read()/fread() do when the destination is memory the program has never touched?
   (readtest 1.0 on the machine: fread() into a 64 KB buffer on a FRESH stack gave a wrong file hash although the byte count was right;
   the same read through read(), or into static/malloc memory, or later on the same stack, was right.)
   Run each case in its own process, so that the stack is fresh:   readtest2 MODE KBYTES [FILE]
     read   KB   read() into a fresh stack buffer, logging every return value (short reads show as values below the request)
     fread  KB   one fread() into a fresh stack buffer
     touch  KB   memset the stack buffer first, then fread()
     mmap   KB   read() loop into a fresh anonymous mmap
     malloc KB   read() loop into a fresh malloc block
     model  KB   (anywhere) a simulated "fread() that does not advance after a short read": shows the page map such a bug leaves
   After each read the data is compared with the file read into ordinary heap memory: the page map says which 4 KB pages of the buffer are right
   (ok), hold a different page of the file (F<n>), are all zero, or are something else (?).  FILE defaults to SharedLibs:lib.armeabihf.libgcc_s/so/1.  */
#define _GNU_SOURCE 1
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef ROTEST_CFG
#define ROTEST_CFG "readtest2"
#endif
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

#define PAGE 4096
static const char *path = "SharedLibs:lib.armeabihf.libgcc_s/so/1";
static unsigned char *truth; static long truth_size;
static long rets[512]; static int nrets;

static void load_truth(void)
{
  int fd = open(path, O_RDONLY);
  if (fd < 0) { printf("cannot open %s\n", path); exit(2); }
  off_t size = lseek(fd, 0, SEEK_END); lseek(fd, 0, SEEK_SET);
  truth = malloc(size); long got = 0; ssize_t n;
  while (got < size && (n = read(fd, truth + got, size - got)) > 0) got += n;
  close(fd);
  truth_size = got;
  if (got != size) { printf("short read of the reference copy: %ld of %ld\n", got, (long) size); exit(2); }
}

static void log_ret(long n) { if (nrets < 512) rets[nrets++] = n; }

static void report(const unsigned char *buf, size_t len, long returned)
{
  printf("  returned: %ld of %zu bytes\n", returned, len);
  if (nrets) {
    printf("  read() returns:");
    for (int i = 0; i < nrets; i++) printf(" %ld", rets[i]);
    printf("%s\n", nrets == 512 ? " ..." : "");
  }
  size_t bad = 0, first = (size_t) -1;
  for (size_t i = 0; i < len && i < (size_t) truth_size; i++) if (buf[i] != truth[i]) { bad++; if (first == (size_t) -1) first = i; }
  if (!bad) { printf("  data: every byte right\n"); return; }
  printf("  data: %zu bytes differ from the file, the first at offset %zu (page %zu)\n", bad, first, first / PAGE);
  printf("  page map (ok = right, F<n> = a copy of file page n, zero, ? = other):\n   ");
  size_t pages = len / PAGE;
  for (size_t p = 0; p < pages; p++) {
    const unsigned char *b = buf + p * PAGE; char label[24];
    if (!memcmp(b, truth + p * PAGE, PAGE)) strcpy(label, "ok");
    else {
      long hit = -1;
      for (long k = 0; k < truth_size / PAGE; k++) if (!memcmp(b, truth + k * PAGE, PAGE)) { hit = k; break; }
      if (hit >= 0) snprintf(label, sizeof label, "F%ld", hit);
      else { size_t z = 0; while (z < PAGE && !b[z]) z++; strcpy(label, z == PAGE ? "zero" : "?"); }
    }
    printf(" %zu=%s", p, label);
    if (p % 12 == 11) printf("\n   ");
  }
  printf("\n");
}

static void stack_info(const void *buf, size_t len)
{
  int local;
  printf("  destination %p..%p, a local variable at %p", buf, (const char *) buf + len, (void *) &local);
  pthread_attr_t a; void *addr = 0; size_t size = 0, guard = 0;
  if (!pthread_getattr_np(pthread_self(), &a)) {
    pthread_attr_getstack(&a, &addr, &size); pthread_attr_getguardsize(&a, &guard);
    printf("; stack %p..%p (%zu bytes, guard %zu)", addr, (char *) addr + size, size, guard);
  }
  printf("\n");
}

static __attribute__((noinline)) void do_stack(const char *mode, size_t len)
{
  unsigned char *buf = __builtin_alloca(len);
  stack_info(buf, len);
  nrets = 0;
  if (!strcmp(mode, "touch")) memset(buf, 0x5a, len);
  if (!strcmp(mode, "fread") || !strcmp(mode, "touch")) {
    FILE *f = fopen(path, "rb"); if (!f) { printf("cannot open\n"); exit(2); }
    size_t n = fread(buf, 1, len, f);
    fclose(f);
    report(buf, len, (long) n);
  } else {
    int fd = open(path, O_RDONLY); if (fd < 0) { printf("cannot open\n"); exit(2); }
    size_t got = 0; ssize_t n;
    while (got < len && (n = read(fd, buf + got, len - got)) > 0) { log_ret(n); got += n; }
    close(fd);
    report(buf, len, (long) got);
  }
}

static void do_heap(const char *mode, size_t len)
{
  unsigned char *buf;
  if (!strcmp(mode, "mmap")) { buf = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); if (buf == MAP_FAILED) { printf("mmap failed\n"); exit(2); } }
  else { buf = malloc(len); if (!buf) { printf("malloc failed\n"); exit(2); } }
  printf("  destination %p..%p (%s)\n", buf, buf + len, mode);
  nrets = 0;
  int fd = open(path, O_RDONLY); if (fd < 0) { printf("cannot open\n"); exit(2); }
  size_t got = 0; ssize_t n;
  while (got < len && (n = read(fd, buf + got, len - got)) > 0) { log_ret(n); got += n; }
  close(fd);
  report(buf, len, (long) got);
}

/* model KB N1KB: what the page map looks like when fread() does not advance its data pointer after a short read: the first read() returned N1KB, the second
   (the rest, into the START of the buffer again) the remaining KB - N1KB, in a zeroed buffer.  Runs anywhere; shows what to look for.  */
static void do_model(size_t len, size_t first)
{
  unsigned char *buf = calloc(1, len);
  memcpy(buf, truth, first);
  memcpy(buf, truth + first, len - first);
  printf("  (model: first read() returned %zu bytes, the second %zu bytes, into the start of the buffer again)\n", first, len - first);
  nrets = 0; log_ret((long) first); log_ret((long) (len - first));
  report(buf, len, (long) len);
}

int main(int argc, char **argv)
{
  if (argc < 3) { printf("usage: readtest2 read|fread|touch|mmap|malloc KBYTES [FILE]\n"); return 2; }
  if (argc > 3) path = argv[3];
  size_t len = (size_t) atoi(argv[2]) * 1024;
  printf("readtest2 1.0 [%s] %s %s KB, file %s\n", ROTEST_CFG, argv[1], argv[2], path);
  load_truth();
  if ((long) len > truth_size || len % PAGE) { printf("the size must be a multiple of 4 KB and at most the file (%ld bytes)\n", truth_size); return 2; }
  if (!strcmp(argv[1], "model")) do_model(len, (argc > 4 ? (size_t) atoi(argv[4]) : (size_t) (len / 2048)) * 1024);
  else if (!strcmp(argv[1], "mmap") || !strcmp(argv[1], "malloc")) do_heap(argv[1], len);
  else do_stack(argv[1], len);
  return 0;
}
