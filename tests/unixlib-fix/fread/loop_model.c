/* loop_model.c -- host model of the direct-transfer loops of UnixLib's fread() and fwrite(), before and after patches-unixlib/unixlib-stdio-short-transfers.patch.
   The loops are copied from libunixlib/stdio/fread.c / fwrite.c (only the stream bookkeeping is dropped); read()/write() are replaced by fakes that return fewer bytes
   than asked for according to a random pattern.  OLD must corrupt the data as soon as a short transfer happens (count right, data wrong, exactly the observed symptom),
   NEW must always be right.  usage: loop_model [CASES]  (default 1000000)  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static const unsigned char *src; static size_t src_len, src_pos; static unsigned char *dst; static size_t dst_pos;
static uint64_t rng = 88172645463325252ull;
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (uint32_t) (rng >> 11); }
static int short_mode;        /* 0: full transfers, 1: random short transfers (never 0 before the end), 2: page-sized pieces */

static long take(size_t want)  /* how many bytes this fake transfer moves */
{
  size_t n = want;
  if (short_mode == 1 && want > 1) n = 1 + rnd() % want;
  if (short_mode == 2 && want > 4096) n = 4096 * (1 + rnd() % (want / 4096));
  return (long) n;
}
static long fake_read(void *buf, size_t want)
{
  size_t n = take(want);
  if (src_pos + n > src_len) n = src_len - src_pos;
  memcpy(buf, src + src_pos, n); src_pos += n; return (long) n;
}
static long fake_write(const void *buf, size_t want)
{
  size_t n = take(want);
  memcpy(dst + dst_pos, buf, n); dst_pos += n; return (long) n;
}

/* fread: the loop of fread.c.  advance=0: before the patch, 1: after.  Returns the number of bytes read. */
static size_t loop_fread(void *data, size_t to_read, int advance)
{
  size_t total = to_read;
  while (to_read) {
    long bytes = fake_read(data, to_read);
    if (bytes == 0) break;
    else if (bytes == -1) break;
    else {
      to_read -= bytes;
      if (advance) data = (void *) ((char *) data + bytes);
    }
  }
  return total - to_read;
}

/* fwrite: the loop of fwrite.c (size_t bytes, -1 test as in the original) */
static size_t loop_fwrite(const void *data, size_t to_write, int advance)
{
  size_t total_bytes = 0;
  while (to_write) {
    size_t bytes = (size_t) fake_write(data, to_write);
    if (bytes == (size_t) -1) break;
    to_write -= bytes;
    if (advance) data = (const void *) ((const char *) data + bytes);
    total_bytes += bytes;
  }
  return total_bytes;
}

int main(int argc, char **argv)
{
  long cases = argc > 1 ? atol(argv[1]) : 1000000;
  long old_bad = 0, new_bad = 0, old_bad_short = 0, old_bad_full = 0, count_wrong = 0, shorts = 0;
  for (long c = 0; c < cases; c++) {
    size_t len = 1 + rnd() % 70000;
    unsigned char *file = malloc(len + 16), *out = calloc(1, len + 16), *copy = malloc(len + 16);
    for (size_t i = 0; i < len; i++) file[i] = (unsigned char) (rnd() >> 3);
    short_mode = (int) (rnd() % 3);
    for (int advance = 0; advance < 2; advance++) {
      /* fread model */
      src = file; src_len = len; src_pos = 0;
      memset(out, 0xA5, len + 16);
      size_t got = loop_fread(out, len, advance);
      int right = got == len && !memcmp(out, file, len);
      if (got != len) count_wrong++;
      if (!advance) { if (!right) { old_bad++; if (short_mode) old_bad_short++; else old_bad_full++; } if (short_mode) shorts++; }
      else if (!right) new_bad++;
      /* fwrite model */
      dst = copy; dst_pos = 0; memset(copy, 0x5A, len + 16);
      size_t put = loop_fwrite(file, len, advance);
      int wright = put == len && !memcmp(copy, file, len);
      if (!advance) { if (!wright) old_bad++; } else if (!wright) new_bad++;
    }
    free(file); free(out); free(copy);
  }
  printf("%ld cases (fread + fwrite loops each): OLD loops gave wrong data in %ld transfers (%ld of them with short transfers, %ld with none), NEW loops in %ld; byte count wrong in %ld\n",
         cases, old_bad, old_bad_short, old_bad_full, new_bad, count_wrong);
  printf("%s\n", new_bad == 0 && old_bad_full == 0 && old_bad > 0 ? "PASS: the old loops corrupt exactly when a transfer is short, the new ones never do" : "FAIL");
  return !(new_bad == 0 && old_bad_full == 0 && old_bad > 0);
}
