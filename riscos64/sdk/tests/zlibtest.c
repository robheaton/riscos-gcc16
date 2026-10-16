/* zlibtest - zlib 1.2.12 built with the SDK as a test of real code: known checksums, compress/uncompress at every level and strategy, a streaming deflate/inflate in small pieces, and a damaged stream.  sdk/tests/build-zlib.sh ZLIBDIR builds it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zlib.h"

static int checks, fails;
#define CHECK(c, what) do { checks++; if (!(c)) { fails++; printf ("FAIL %s (line %d)\n", what, __LINE__); } else printf ("PASS %s\n", what); } while (0)

static unsigned rng = 12345;
static unsigned next (void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

int main (void)
{
  enum { N = 300000 };
  unsigned char *src = malloc (N), *comp, *back;
  uLongf clen, blen;
  int level, strat, i, err;
  unsigned long total = 0;
  printf ("RISC OS 64 test zlibtest (zlib %s)\n", zlibVersion ());
  CHECK (crc32 (0, (const Bytef *) "123456789", 9) == 0xCBF43926UL, "crc32 of 123456789");
  CHECK (adler32 (1, (const Bytef *) "Wikipedia", 9) == 0x11E60398UL, "adler32 of Wikipedia");
  for (i = 0; i < N; i++) src[i] = (i % 97 < 60) ? "the quick brown fox jumps over the lazy dog "[i % 44] : (unsigned char) next ();
  comp = malloc (compressBound (N)); back = malloc (N);
  for (level = 0; level <= 9; level++)
    {
      clen = compressBound (N); blen = N;
      err = compress2 (comp, &clen, src, N, level);
      if (err == Z_OK) err = uncompress (back, &blen, comp, clen);
      total += clen;
      if (err != Z_OK || blen != N || memcmp (src, back, N)) { CHECK (0, "round trip at a level"); printf ("INFO level %d err %d\n", level, err); }
    }
  CHECK (total > 0, "compress and uncompress at levels 0 to 9 round-trip 300000 bytes");
  printf ("INFO the sizes of levels 0-9 add up to %lu\n", total);
  for (strat = 0; strat <= 4; strat++)
    {
      z_stream d, s;
      int ok = 1;
      memset (&d, 0, sizeof d); memset (&s, 0, sizeof s);
      deflateInit2 (&d, 6, Z_DEFLATED, 15, 8, strat);
      clen = compressBound (N);
      d.next_in = src; d.avail_in = N; d.next_out = comp; d.avail_out = (uInt) clen;
      if (deflate (&d, Z_FINISH) != Z_STREAM_END) ok = 0;
      clen = d.total_out; deflateEnd (&d);
      inflateInit (&s);
      s.next_in = comp; s.next_out = back;                                    /* inflate in pieces of 1000 bytes in and 777 out */
      { uLong in = 0, out = 0; int r = Z_OK;
        while (r == Z_OK) { uInt gi = (uInt) (clen - in > 1000 ? 1000 : clen - in), go = (uInt) (N - out > 777 ? 777 : N - out);
          s.next_in = comp + in; s.avail_in = gi; s.next_out = back + out; s.avail_out = go;
          r = inflate (&s, Z_NO_FLUSH); in += gi - s.avail_in; out += go - s.avail_out; if (r == Z_BUF_ERROR && gi == 0 && go == 0) break; }
        if (r != Z_STREAM_END || out != N || memcmp (src, back, N)) ok = 0; }
      inflateEnd (&s);
      CHECK (ok, "streaming deflate (strategy 0-4) and inflate in small pieces");
    }
  comp[clen / 2] ^= 0xFF;
  blen = N;
  CHECK (uncompress (back, &blen, comp, clen) != Z_OK, "a damaged stream is refused");
  printf ("SUMMARY %d checks, %d failed\n", checks, fails);
  return fails != 0;
}
