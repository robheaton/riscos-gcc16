/* t03int - integer arithmetic on a 64-bit machine.  The expected output is made by building this same file for the host. */
#include "t64.h"

static u64 lcg (u64 *s) { *s = *s * 6364136223846793005UL + 1442695040888963407UL; return *s; }
static u32 crc32 (const u8 *p, u64 n)
{
  u32 c = 0xFFFFFFFFu; u64 i; int k;
  for (i = 0; i < n; i++) { c ^= p[i]; for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1))); }
  return ~c;
}
static u64 rol (u64 v, int n) { n &= 63; return n ? (v << n) | (v >> (64 - n)) : v; }
static u64 sdiv_hash (i64 a, i64 b) { return (u64) (a / b) ^ rol ((u64) (a % b), 17); }
static void shellsort (i64 *a, int n)
{
  int gap, i, j;
  for (gap = n / 2; gap > 0; gap /= 2)
    for (i = gap; i < n; i++) { i64 t = a[i]; for (j = i; j >= gap && a[j - gap] > t; j -= gap) a[j] = a[j - gap]; a[j] = t; }
}
static int parse_dec (const char *s, i64 *out)
{
  i64 v = 0; int neg = 0;
  if (*s == '-') { neg = 1; s++; }
  if (!*s) return 0;
  while (*s) { if (*s < '0' || *s > '9') return 0; v = v * 10 + (*s - '0'); s++; }
  *out = neg ? -v : v; return 1;
}
struct mix { u8 a; u64 b; u8 c; u32 d; };

int main (void)
{
  u64 s = 12345, h = 0, i;
  u8 buf[64], buf2[64];
  i64 arr[100];
  int ok;

  t_puts ("RISC OS 64 test t03int\n");
  CHECK (sizeof (void *) == 8 && sizeof (u64) == 8 && sizeof (u32) == 4 && sizeof (int) == 4);
  CHECK (sizeof (struct mix) == 24);
  CHECK (((u64) &buf & 0) == 0);

  /* constants and shifts */
  CHECK (0x123456789ABCDEF0UL >> 60 == 1);
  CHECK ((0x8000000000000000UL >> 63) == 1);
  CHECK (((i64) -1 >> 10) == -1);
  CHECK ((1UL << 63) != 0 && (1UL << (64 - 1)) == 0x8000000000000000UL);
  CHECK (rol (0x8000000000000001UL, 1) == 3);
  CHECK (0xFFFFFFFFFFFFFFFFUL + 1 == 0);

  /* multiply, divide, modulo, signed and unsigned: a hash of a thousand operations, printed every 250 */
  for (i = 0; i < 1000; i++)
    {
      u64 a = lcg (&s), b = lcg (&s) | 1;
      i64 sa = (i64) (a >> 1), sb = (i64) ((b >> 3) | 1) * ((i & 1) ? -1 : 1);
      h ^= rol (a * b, (int) i) ^ (a / b) ^ rol (a % b, 7);
      h += sdiv_hash (sa, sb);
      h ^= rol (a >> (i & 63), 3) + (a << (i & 63));
      h = h * 0x9E3779B97F4A7C15UL + (u64) __builtin_clzl (a | 1) + (u64) __builtin_popcountl (b);
      if (i % 250 == 249) { t_puts ("hash after "); t_dec ((i64) i + 1); t_puts (" operations: "); t_hex (h, 16); t_putc ('\n'); }
    }

  /* a 128 bit product: the high half */
  {
    unsigned __int128 p = (unsigned __int128) 0xFEDCBA9876543210UL * 0x0123456789ABCDEFUL;
    t_puts ("128 bit product: "); t_hex ((u64) (p >> 64), 16); t_hex ((u64) p, 16); t_putc ('\n');
    CHECK ((u64) p == 0xFEDCBA9876543210UL * 0x0123456789ABCDEFUL);
  }

  /* memory functions */
  for (i = 0; i < 64; i++) buf[i] = (u8) (i * 7 + 3);
  memcpy (buf2, buf, 64); CHECK (memcmp (buf, buf2, 64) == 0);
  memmove (buf + 5, buf, 40); CHECK (buf[5] == 3 && buf[44] == (u8) (39 * 7 + 3) && buf[4] == (u8) (4 * 7 + 3));
  memset (buf2, 0xA5, 64); CHECK (buf2[0] == 0xA5 && buf2[63] == 0xA5);
  t_puts ("crc32 of the buffer: "); t_hex (crc32 (buf, 64), 8); t_putc ('\n');
  CHECK (crc32 ((const u8 *) "123456789", 9) == 0xCBF43926u);
  CHECK (strlen ("RISC OS 64") == 10);

  /* sorting and number parsing */
  for (i = 0; i < 100; i++) arr[i] = (i64) (lcg (&s) >> 40) - (1L << 23);
  shellsort (arr, 100);
  ok = 1; for (i = 1; i < 100; i++) if (arr[i - 1] > arr[i]) ok = 0;
  CHECK (ok);
  t_puts ("smallest "); t_dec (arr[0]); t_puts (", largest "); t_dec (arr[99]); t_putc ('\n');
  {
    i64 v = 0;
    CHECK (parse_dec ("9223372036854775807", &v) && v == 0x7FFFFFFFFFFFFFFFL);
    CHECK (parse_dec ("-12345", &v) && v == -12345);
    CHECK (!parse_dec ("12x", &v));
    t_dec (-9223372036854775807L - 1); t_putc ('\n');
  }

  /* a function pointer table and an indirect call (the calling convention) */
  {
    static u64 (*const f[3]) (u64, int) = { rol, (u64 (*) (u64, int)) 0, rol };
    CHECK (f[0] (1, 4) == 16 && f[2] (3, 63) == 0x8000000000000001UL);
  }
  return t_summary ("t03int");
}
