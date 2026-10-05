/* fread_model.c -- can the corrupted hash that readtest printed on the machine (FNV 6481ff0a instead of 4fce8f52, same length) be explained by UnixLib's fread()
   not advancing its data pointer after a short read()?  Model: only the FIRST 64 KB fread of the file is hit; its read() calls returned r1, r2, ... bytes (page multiples,
   summing to 65536); each next read() went to buf+0 again; the buffer started out zeroed (fresh stack).  The hash of the whole file is then checked against the observed one.
   usage: fread_model FILE OBSERVED_HASH [CHUNK=65536] [GRANULE=4096] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static uint32_t fnv(uint32_t h, const unsigned char *p, size_t n) { while (n--) { h ^= *p++; h *= 16777619u; } return h; }

int main(int argc, char **argv)
{
  if (argc < 3) { fprintf(stderr, "usage\n"); return 2; }
  FILE *f = fopen(argv[1], "rb"); if (!f) { perror(argv[1]); return 2; }
  uint32_t observed = (uint32_t) strtoul(argv[2], 0, 16);
  size_t chunk = argc > 3 ? strtoul(argv[3], 0, 0) : 65536, gran = argc > 4 ? strtoul(argv[4], 0, 0) : 4096;
  fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
  unsigned char *F = malloc(size); if (fread(F, 1, size, f) != (size_t) size) return 2;
  uint32_t good = fnv(2166136261u, F, size);
  printf("file %ld bytes, correct FNV %08x, observed %08x\n", size, good, observed);
  /* invert the tail (everything after the first chunk) from the observed final hash: the state after the first chunk that would give it */
  uint32_t inv = 1; /* inverse of 16777619 mod 2^32 by Newton iteration */
  for (int i = 0; i < 6; i++) inv *= 2 - 16777619u * inv;
  uint32_t s = observed;
  for (long i = size - 1; i >= (long) chunk; i--) s = (s * inv) ^ F[i];
  printf("state needed after the first %zu bytes: %08x\n", chunk, s);
  size_t pages = chunk / gran; long found = 0;
  unsigned char *buf = malloc(chunk);
  for (uint32_t mask = 0; mask < (1u << (pages - 1)); mask++) {          /* bit i set: a read() ends after page i+1 (a split point) */
    memset(buf, 0, chunk);
    size_t pos = 0, start = 0; int nreads = 0; size_t r[64];
    for (size_t p = 1; p <= pages; p++) {
      if (p == pages || (mask >> (p - 1) & 1)) { size_t n = (p - start) * gran; memcpy(buf, F + pos, n); pos += n; r[nreads++] = n; start = p; }
    }
    if (fnv(2166136261u, buf, chunk) == s) {
      found++; printf("MATCH: reads of"); for (int i = 0; i < nreads; i++) printf(" %zu", r[i]); printf(" bytes\n");
    }
  }
  printf("%ld matching read sequences (page-granular, zeroed start)\n", found);
  return 0;
}
