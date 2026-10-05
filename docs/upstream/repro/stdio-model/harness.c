/* Host model of the direct-transfer loops of UnixLib's fread () and fwrite () - the REAL stdio/fread.c and stdio/fwrite.c, compiled on the host against a mock read () / write ()
   that return fewer bytes than asked for (a socket, a terminal, a full pipe).  What must hold: the bytes arrive in order, the count is right, whatever the pattern of short transfers.
   usage: harness [CASES]   (default 200000; the exit status is the number of failed cases)  */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <pthread.h>
#include "stdio_struct.h"              /* typedef struct __iobuf FILE is the stub's job in fread.c; here the struct is used under the name MFILE */

typedef struct __iobuf MFILE;
struct ul_global { int fls_lbstm_on_rd; };
struct ul_global __ul_global;
#define _IOMAGIC 0x4f4d4f44u
extern size_t ul_fread (void *, size_t, size_t, MFILE *);
extern size_t ul_fwrite (const void *, size_t, size_t, MFILE *);

/* ---- the mock transfers ---- */
static uint64_t rng = 88172645463325252ull;
static uint32_t rnd (void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (uint32_t) (rng >> 11); }
static int mode;                                   /* 0 full, 1 random short, 2 one byte at a time (small sizes), 3 pieces of 4096 */
static const unsigned char *src; static size_t src_len, src_pos;
static unsigned char *dst; static size_t dst_len, dst_cap;
static long nshort;

static size_t piece (size_t want)
{
  size_t n = want;
  if (mode == 1 && want > 1) n = 1 + rnd () % want;
  else if (mode == 2) n = 1;
  else if (mode == 3 && want > 4096) n = 4096 * (1 + rnd () % (want / 4096));
  if (n < want) nshort++;
  return n;
}
ssize_t model_read (int fd, void *buf, size_t want)
{
  (void) fd;
  size_t n = piece (want);
  if (src_pos + n > src_len) n = src_len - src_pos;                  /* the end of the source: 0 = EOF */
  memcpy (buf, src + src_pos, n); src_pos += n;
  return (ssize_t) n;
}
ssize_t model_write (int fd, const void *buf, size_t want)
{
  (void) fd;
  size_t n = piece (want);
  if (dst_len + n > dst_cap) n = dst_cap - dst_len;
  memcpy (dst + dst_len, buf, n); dst_len += n;
  return (ssize_t) n;
}
int model_isatty (int fd) { (void) fd; return 0; }
int __flslbbuf (void) { return 0; }
int __flsbuf (int c, MFILE *s)                                      /* flush the output buffer through the same mock write (a full-size write loop of its own) */
{
  (void) c;
  size_t n = (size_t) (s->o_ptr - s->o_base), done = 0;
  while (done < n) { ssize_t w = model_write (s->fd, s->o_base + done, n - done); if (w <= 0) return -1; done += (size_t) w; }
  s->__offset += (long) n;
  s->o_ptr = s->o_base; s->o_cnt = (int) s->__bufsize;
  return 0;
}

static int bad; static long cases, shorts;
#define CHECK(c, ...) do { if (!(c)) { if (bad++ < 12) { printf ("FAIL case %ld (mode %d, line %d): ", cases, mode, __LINE__); printf (__VA_ARGS__); printf ("\n"); } } } while (0)

static MFILE mk (int fd) { MFILE f; memset (&f, 0, sizeof f); f.__magic = _IOMAGIC; f.fd = fd; return f; }

/* fread: unbuffered / buffered with a prefilled buffer / with a pushed back character / size > 1 / source shorter than the request */
static void test_fread (void)
{
  size_t len = 1 + rnd () % (mode == 2 ? 300 : 70000);
  unsigned char *file = malloc (len + 64), *out = malloc (len + 64), *ibuf = malloc (4096);
  for (size_t i = 0; i < len; i++) file[i] = (unsigned char) (rnd () >> 3);
  int kind = (int) (rnd () % 4);
  size_t size = 1, want = len + (kind == 3 ? rnd () % 50 : 0);       /* kind 3: ask for more than the source has */
  MFILE f = mk (5); f.__mode.__bits.__read = 1;
  size_t prefilled = 0, pushed = 0;
  src = file; src_len = len; src_pos = 0; unsigned char first = 0;
  if (kind == 1 || kind == 2) {                                        /* buffered stream with k bytes already in the buffer */
    prefilled = len > 1 ? rnd () % (len < 4096 ? len : 4096) : 0;
    memcpy (ibuf, file, prefilled); src_pos = prefilled;
    f.i_base = ibuf; f.i_ptr = ibuf; f.i_cnt = (int) prefilled; f.__bufsize = 4096;
  }
  if (kind == 2) {                                                     /* ungetc: one character pushed back; fread () restores i_cnt from __pushedi_cnt and delivers it first */
    pushed = 1; first = 0xEE; f.__pushedback = 1; f.__pushedchar = first; f.__pushedi_cnt = (int) prefilled; f.i_cnt = 0;
  }
  memset (out, 0xA5, len + 64);
  size_t got = ul_fread (out, size, want, &f);
  size_t expect = len;                                                 /* all the bytes the source holds, the pushed back one counts as the first */
  if (pushed) { /* the delivered stream is: first, buffer (= file[0..prefilled) shifted: file bytes), then the rest; so it has len + 1 bytes when the source holds len bytes */ expect = len + 1; }
  if (expect > want) expect = want;
  CHECK (got == expect, "fread returned %zu, expected %zu (kind %d, len %zu)", got, expect, kind, len);
  size_t cmpn = got < expect ? got : expect;
  if (!pushed) CHECK (!memcmp (out, file, cmpn), "fread data wrong (kind %d, len %zu, %zu bytes compared)", kind, len, cmpn);
  else { CHECK (out[0] == first, "pushed back character lost"); CHECK (!memcmp (out + 1, file, cmpn - 1), "fread data wrong after the pushed back character (len %zu)", len); }
  free (file); free (out); free (ibuf);
}

/* fwrite: unbuffered / buffered with a direct write (more than the buffer holds) / buffered small write */
static void test_fwrite (void)
{
  size_t len = 1 + rnd () % (mode == 2 ? 300 : 70000);
  unsigned char *data = malloc (len + 64), *obuf = malloc (4096);
  for (size_t i = 0; i < len; i++) data[i] = (unsigned char) (rnd () >> 3);
  dst_cap = len + 5000; dst = malloc (dst_cap + 16); dst_len = 0; memset (dst, 0x5A, dst_cap + 16);
  int kind = (int) (rnd () % 3);
  MFILE f = mk (6); f.__mode.__bits.__write = 1;
  size_t prior = 0;
  if (kind != 0) {                                                     /* buffered stream, kind 2: with some data already buffered */
    f.o_base = obuf; f.o_ptr = obuf; f.o_cnt = 4096; f.__bufsize = 4096;
    if (kind == 2) { prior = rnd () % 100; for (size_t i = 0; i < prior; i++) *f.o_ptr++ = (unsigned char) ('a' + i % 26); f.o_cnt -= (int) prior; }
  }
  size_t put = ul_fwrite (data, 1, len, &f);
  CHECK (put == len, "fwrite returned %zu for %zu bytes (kind %d)", put, len, kind);
  if (kind != 0) __flsbuf (EOF, &f);                                   /* push out what is still buffered */
  CHECK (dst_len == len + prior, "the file got %zu bytes, expected %zu (kind %d)", dst_len, len + prior, kind);
  if (dst_len == len + prior) {
    CHECK (!memcmp (dst + prior, data, len), "fwrite data wrong (kind %d, len %zu, prior %zu)", kind, len, prior);
  }
  free (data); free (obuf); free (dst);
}

int main (int argc, char **argv)
{
  long n = argc > 1 ? atol (argv[1]) : 200000;
  for (cases = 0; cases < n; cases++) {
    mode = getenv ("MODEL_FULL_ONLY") ? 0 : (int) (rnd () % 4); nshort = 0;
    test_fread (); shorts += nshort > 0;
    nshort = 0; test_fwrite (); shorts += nshort > 0;
  }
  printf ("%s: %ld cases (a fread () and a fwrite () each; %ld of those transfers saw a short read or write): %d failed check(s)\n", getenv ("MODEL_LABEL") ? getenv ("MODEL_LABEL") : "model", n, shorts, bad);
  return bad != 0;
}
