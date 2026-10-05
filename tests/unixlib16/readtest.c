/* NOTE: the GCC 16 port compiles with -fstack-clash-protection by default, which maps the stack pages before this program reaches them: build it with -fno-stack-clash-protection to see the problem it probes. */
/* readtest.c -- does reading a file give the same bytes whatever the size and place of the buffer?  (ulinfo 1.2, which read the library with a 64 KB buffer ON THE STACK, once
   reported a hash that ulinfo 1.3, with a 4 KB static buffer, did not.)  Reads one file several ways and prints the FNV-1a hash of each; all must agree.
   usage: readtest [file]   default: SharedLibs:lib.armeabihf.libgcc_s/so/1 (1480628 bytes, FNV 4fce8f52)  */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef ROTEST_CFG
#define ROTEST_CFG "readtest"
#endif

static unsigned fnv_add(unsigned h, const unsigned char *p, size_t n) { while (n--) { h ^= *p++; h *= 16777619u; } return h; }

static unsigned char static_buf[65536];

static int via_fread(const char *path, unsigned char *buf, size_t bufsz, unsigned long *total, unsigned *hash)
{
  FILE *f = fopen(path, "rb"); if (!f) return 0;
  size_t n; *total = 0; *hash = 2166136261u;
  while ((n = fread(buf, 1, bufsz, f)) > 0) { *total += n; *hash = fnv_add(*hash, buf, n); }
  fclose(f); return 1;
}

static int via_read(const char *path, unsigned char *buf, size_t bufsz, unsigned long *total, unsigned *hash)
{
  int fd = open(path, O_RDONLY); if (fd < 0) return 0;
  ssize_t n; *total = 0; *hash = 2166136261u;
  while ((n = read(fd, buf, bufsz)) > 0) { *total += n; *hash = fnv_add(*hash, buf, n); }
  close(fd); return 1;
}

static int stack_fread(const char *path, unsigned long *total, unsigned *hash) { unsigned char buf[65536]; return via_fread(path, buf, sizeof buf, total, hash); }
static int stack_read(const char *path, unsigned long *total, unsigned *hash) { unsigned char buf[65536]; return via_read(path, buf, sizeof buf, total, hash); }
static int stack_fread_small(const char *path, unsigned long *total, unsigned *hash) { unsigned char buf[4096]; return via_fread(path, buf, sizeof buf, total, hash); }

int main(int argc, char **argv)
{
  const char *path = argc > 1 ? argv[1] : "SharedLibs:lib.armeabihf.libgcc_s/so/1";
  printf("readtest 1.0 [%s] file %s\n", ROTEST_CFG, path);
  unsigned long t[8]; unsigned h[8]; int ok[8]; const char *what[8] = {
    "fread, 4 KB buffer on the stack", "fread, 64 KB buffer on the stack", "read(),  64 KB buffer on the stack", "fread, 64 KB static buffer", "read(),  64 KB static buffer",
    "fread, 64 KB buffer from malloc", "read(),  1 MB buffer from malloc", "fread, 512 byte static buffer" };
  ok[0] = stack_fread_small(path, &t[0], &h[0]);
  ok[1] = stack_fread(path, &t[1], &h[1]);
  ok[2] = stack_read(path, &t[2], &h[2]);
  ok[3] = via_fread(path, static_buf, sizeof static_buf, &t[3], &h[3]);
  ok[4] = via_read(path, static_buf, sizeof static_buf, &t[4], &h[4]);
  unsigned char *m = malloc(65536); ok[5] = m && via_fread(path, m, 65536, &t[5], &h[5]); free(m);
  unsigned char *m1 = malloc(1 << 20); ok[6] = m1 && via_read(path, m1, 1 << 20, &t[6], &h[6]); free(m1);
  ok[7] = via_fread(path, static_buf, 512, &t[7], &h[7]);
  int bad = 0; unsigned ref = 0; int have = 0;
  for (int i = 0; i < 8; i++) {
    if (!ok[i]) { printf("  %-34s could not open the file\n", what[i]); bad++; continue; }
    if (!have) { ref = h[i]; have = 1; }
    printf("  %-34s %lu bytes, FNV %08x%s\n", what[i], t[i], h[i], h[i] == ref ? "" : "   <-- DIFFERENT");
    if (h[i] != ref) bad++;
  }
  printf("SUMMARY [readtest]: %s\n", bad ? "FAIL: the same file gave different bytes" : "PASS: every way of reading gives the same bytes");
  return bad != 0;
}
