/* libtest.c - tests of the C library functions of modkit (modkit/lib), as one program that is built three ways and run with the same inputs; each section prints  SECTION name n=<records> hash=<FNV of every result>
   and the three builds must print the same lines:

     T_ORACLE   the host's C library (glibc) answers: the reference.  -DEMU_LONG_BITS=32 makes it answer as a 32 bit long (ARM) would.
     T_HOSTLIB  the library's own sources compiled for the host (with ASan and UBSan) and renamed mk_*: see mk_rename.h; the test calls them under the standard names
     T_ARM      the library as ARM code (libmodkit.a), run on the interpreter (tests/libtest/armrun.py) or, in a module, on the machine
     T_HW       (with T_ARM) a runnable module for the machine: the sections that need the test SWIs of the interpreter are left out, the others are compared with the hashes of glibc's run (expected.h:
                made by pack/make-pack32.py from the oracle's output with the same SCALE), and the section hw uses the real SWIs

   The inputs come from a generator of this file (xorshift32), identical in every build; the work is scaled by SCALE.  With  -v  (host) every result is printed, to find the first difference with diff.
   The test code itself uses only its own t_* helpers for strings and memory, never the functions under test. */
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <locale.h>
#if defined (T_HOSTLIB)
# include "mk_rename.h"
# include "mk_decl.h"
extern int mk_errno;
# define T_ERRNO mk_errno
#else
# define T_ERRNO errno
#endif
#ifndef SCALE
#define SCALE 1
#endif
#ifndef EMU_LONG_BITS
#define EMU_LONG_BITS (8 * (int) sizeof (long))
#endif

/* ---- the results: a hash of everything, and in verbose mode the text ---- */
static unsigned g_hash, g_count, g_fail;
static int g_verbose;
static const char *g_sect;
static char g_line[400];
static size_t g_ll;

static size_t t_len (const char *s) { size_t n = 0; while (s[n]) n++; return n; }
static void t_cpy (char *d, const char *s) { while ((*d++ = *s++)) ; }
static void t_set (void *p, int c, size_t n) { unsigned char *q = p; while (n--) *q++ = (unsigned char) c; }
static void t_mov (void *d, const void *s, size_t n) { unsigned char *a = d; const unsigned char *b = s; while (n--) *a++ = *b++; }
static void t_hash (unsigned char b) { g_hash = (g_hash ^ b) * 16777619u; }
static void vcat (const char *s) { while (*s && g_ll < sizeof g_line - 2) g_line[g_ll++] = *s++; }
static void vnum (long long v, int base, int width)
{
  char tmp[24], *p = tmp + sizeof tmp;
  unsigned long long u = v < 0 && base == 10 ? 0 - (unsigned long long) v : (unsigned long long) v;
  *--p = 0;
  do *--p = "0123456789abcdef"[u % (unsigned) base]; while (u /= (unsigned) base);
  while (tmp + sizeof tmp - 1 - p < width) *--p = '0';
  if (v < 0 && base == 10) *--p = '-';
  vcat (p);
}
unsigned g_rng = 1;
static void sect_begin (const char *name, unsigned seed)
{
  g_sect = name; g_hash = 2166136261u; g_count = 0; g_ll = 0; g_rng = seed;
}
static unsigned rnd (void) { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
static unsigned rr (unsigned n) { return rnd () % n; }
/* a random int whose magnitude is cut down by a random shift (two calls in one expression would be evaluated in an unspecified order) */
static int rshift (unsigned maxshift) { int v = (int) rnd (); unsigned s = rr (maxshift); return v >> s; }
static void rec_u (unsigned long long v) { for (int i = 0; i < 8; i++) t_hash ((unsigned char) (v >> (8 * i))); g_count++; if (g_verbose) { vcat (" "); vnum ((long long) v, 16, 0); } }
static void rec_i (long long v) { for (int i = 0; i < 8; i++) t_hash ((unsigned char) ((unsigned long long) v >> (8 * i))); g_count++; if (g_verbose) { vcat (" "); vnum (v, 10, 0); } }
static void rec_s (const char *s)
{
  if (g_verbose) { vcat (" \""); for (const char *q = s; *q; q++) { if (*q >= 32 && *q < 127 && *q != '"' && *q != '\\') { char c[2] = { *q, 0 }; vcat (c); } else { vcat ("\\x"); vnum ((unsigned char) *q, 16, 2); } } vcat ("\""); }
  while (*s) t_hash ((unsigned char) *s++);
  t_hash (0); g_count++;
}
static void rec_m (const void *p, size_t n)
{
  const unsigned char *q = p;
  if (g_verbose) { vcat (" <"); for (size_t i = 0; i < n; i++) vnum (q[i], 16, 2); vcat (">"); }
  for (size_t i = 0; i < n; i++) t_hash (q[i]);
  g_count++;
}
#if defined (T_HW)
struct hw_exp { const char *name; unsigned count, hash; };
static const struct hw_exp hw_expected[] = {
# include "expected.h"
};
#endif
static void case_begin (unsigned i) { g_ll = 0; if (g_verbose) { vcat (g_sect); vcat ("["); vnum (i, 10, 0); vcat ("]"); } }
static void case_end (void) { if (g_verbose) { g_line[g_ll] = 0; puts (g_line); } g_ll = 0; }
static void check (int ok, const char *what)
{
  if (!ok) { g_fail++; char b[160] = "FAIL "; t_cpy (b + 5, g_sect); size_t n = t_len (b); b[n++] = ' '; t_cpy (b + n, what); puts (b); }
  g_count++;
}
static void sect_end (void)
{
  char b[96]; size_t n = 0;
  t_cpy (b, "SECTION "); n = 8; t_cpy (b + n, g_sect); n += t_len (g_sect);
  t_cpy (b + n, " n="); n += 3;
  { char d[12]; int k = 0; unsigned v = g_count; do d[k++] = (char) ('0' + v % 10); while (v /= 10); while (k) b[n++] = d[--k]; }
  t_cpy (b + n, " hash="); n += 6;
  for (int i = 28; i >= 0; i -= 4) b[n++] = "0123456789abcdef"[(g_hash >> i) & 15];
  b[n] = 0;
  puts (b);
#if defined (T_HW)
  for (unsigned i = 0; i < sizeof hw_expected / sizeof hw_expected[0]; i++)
    {
      const char *a = hw_expected[i].name, *c = g_sect;
      while (*a && *a == *c) { a++; c++; }
      if (*a || *c) continue;
      if (hw_expected[i].count == g_count && hw_expected[i].hash == g_hash) puts ("    same as glibc's");
      else { g_fail++; puts ("    DIFFERENT from glibc's (n or hash)"); }
    }
#endif
}
static int sgn (long long v) { return (v > 0) - (v < 0); }
static unsigned N (unsigned base) { return base * SCALE; }

/* ======================================================================== ctype */
static void test_ctype (void)
{
  sect_begin ("ctype", 1);
  for (int c = -1; c <= 255; c++)
    {
      case_begin ((unsigned) (c + 1));
      rec_i (c);
      rec_i ((isalnum) (c) != 0); rec_i ((isalpha) (c) != 0); rec_i ((iscntrl) (c) != 0); rec_i ((isdigit) (c) != 0);
      rec_i ((isgraph) (c) != 0); rec_i ((islower) (c) != 0); rec_i ((isprint) (c) != 0); rec_i ((ispunct) (c) != 0);
      rec_i ((isspace) (c) != 0); rec_i ((isupper) (c) != 0); rec_i ((isxdigit) (c) != 0); rec_i ((isblank) (c) != 0);
      rec_i ((isascii) (c) != 0); rec_i ((toupper) (c)); rec_i ((tolower) (c)); rec_i ((toascii) (c));
      case_end ();
    }
  sect_end ();
}

/* ======================================================================== string */
#define ALPHA "abcABC xy-,:.\t1"
static void rstr (char *d, unsigned size, unsigned maxlen)
{
  unsigned n = rr (maxlen);
  t_set (d, 0x5A, size);
  for (unsigned i = 0; i < n; i++) d[i] = ALPHA[rr (sizeof ALPHA - 1)];
  d[n] = 0;
}
static long off (const char *base, const char *p) { return p ? (long) (p - base) : -1; }
#if defined (T_ORACLE)
# define t_stricmp strcasecmp
# define t_strnicmp strncasecmp
#else
# define t_stricmp stricmp
# define t_strnicmp strnicmp
#endif
static void test_string (void)
{
  sect_begin ("string", 2);
  for (unsigned i = 0; i < N (400); i++)
    {
      char a[48], b[48], set[16], buf[96], buf2[96], tok[96];
      case_begin (i);
      rstr (a, sizeof a, 24); rstr (b, sizeof b, 12); rstr (set, sizeof set, 5);
      int c = rr (6) == 0 ? 0 : rr (8) == 0 ? 'z' : ALPHA[rr (sizeof ALPHA - 1)];
      unsigned n = rr (30), k = rr (40);
      rec_s (a); rec_s (b); rec_s (set); rec_i (c); rec_i (n);
      rec_i ((long) strlen (a)); rec_i ((long) strnlen (a, n));
      rec_i (sgn (strcmp (a, b))); rec_i (sgn (strncmp (a, b, n))); rec_i (sgn (strcasecmp (a, b))); rec_i (sgn (strncasecmp (a, b, n)));
      rec_i (sgn (t_stricmp (a, b))); rec_i (sgn (t_strnicmp (a, b, n))); rec_i (sgn (strcoll (a, b)));
      rec_i (off (a, strchr (a, c))); rec_i (off (a, strrchr (a, c))); rec_i (off (a, strstr (a, b))); rec_i (off (a, memchr (a, c, n)));
      rec_i ((long) strspn (a, set)); rec_i ((long) strcspn (a, set)); rec_i (off (a, strpbrk (a, set)));
      t_set (buf, 0xA5, sizeof buf); strcpy (buf, a); rec_m (buf, 48);
      t_set (buf, 0xA5, sizeof buf); strncpy (buf, b, n); rec_m (buf, 40);
      t_set (buf, 0xA5, sizeof buf); t_cpy (buf, a); strcat (buf, b); rec_m (buf, 48);
      t_set (buf, 0xA5, sizeof buf); t_cpy (buf, a); strncat (buf, b, n); rec_m (buf, 48);
      t_set (buf, 0xA5, sizeof buf); rec_i ((long) strlcpy (buf, a, n)); rec_m (buf, 40);
      t_set (buf, 0xA5, sizeof buf); t_cpy (buf, b); rec_i ((long) strlcat (buf, a, n)); rec_m (buf, 48);
      t_set (buf, 0xA5, sizeof buf); { size_t r = strxfrm (buf, a, n); rec_i ((long) r); if (n > r) rec_m (buf, 40); }      /* a truncated result is indeterminate */
      t_mov (buf, a, 48); t_set (buf2, 0xA5, sizeof buf2); memcpy (buf2, buf, k); memcpy (buf2 + k, buf, 8); rec_m (buf2, 48);
      t_mov (buf, a, 48); { unsigned dx = rr (8), sx = rr (8); memmove (buf + dx, buf + sx, k); } rec_m (buf, 48);
      t_mov (buf, a, 48); memset (buf + (k & 7), c, k); rec_m (buf, 48);
      rec_i (sgn (memcmp (a, b, k)));
      t_set (buf2, 0xA5, sizeof buf2); rec_i (off (buf2, memccpy (buf2, a, c, n))); rec_m (buf2, 40);
      t_mov (buf, a, 48); bzero (buf + 3, k & 15); rec_m (buf, 48);
      t_mov (buf, a, 48); bcopy (buf + 2, buf + 5, k & 15); rec_m (buf, 48);
      /* tokens */
      t_cpy (tok, a);
      { int cnt = 0; char *t = strtok (tok, set); while (t && cnt < 40) { rec_s (t); cnt++; t = strtok (0, set); } rec_i (cnt); }
      t_cpy (tok, a);
      { char *save = 0; int cnt = 0; char *t = strtok_r (tok, set, &save); while (t && cnt < 40) { rec_s (t); cnt++; t = strtok_r (0, set, &save); } rec_i (cnt); }
      t_cpy (tok, a);
      { char *p = tok; int cnt = 0; char *t; while (cnt < 40 && (t = strsep (&p, set)) != 0) { rec_s (t); cnt++; } rec_i (cnt); }
      { char *d = strdup (a); rec_s (d); free (d); d = strndup (a, n); rec_s (d); free (d); }
      case_end ();
    }
  sect_end ();
}

/* ======================================================================== numbers */
static long epos (const char *d, const char *e) { return e ? (long) (e - d) : -2; }
static int norm_err (void) { return T_ERRNO == ERANGE ? 1 : T_ERRNO == EINVAL ? 2 : T_ERRNO ? 3 : 0; }
/* a text that looks like a number, possibly: blanks, a sign, a prefix, up to MAXD digits (of the first NSYM symbols of 0-9a-z, in either case), and a few characters that are not part of it */
static void gen_num (char *d, unsigned maxd)
{
  static const char sym[] = "0123456789abcdefghijklmnopqrstuvwxyz";
  char *p = d;
  unsigned ws = rr (3), s = rr (4), pre = rr (7), nd = rr (maxd + 1), nsym = 2 + rr (35), j = rr (3);
  while (ws--) *p++ = " \t\n\v\f\r"[rr (6)];
  if (s == 1) *p++ = '+'; else if (s == 2) *p++ = '-';
  if (pre == 1) *p++ = '0'; else if (pre == 2) { *p++ = '0'; *p++ = 'x'; } else if (pre == 3) { *p++ = '0'; *p++ = 'X'; }
  while (nd--) { char c = sym[rr (nsym)]; if (c > '9' && rr (2)) c = (char) (c - 32); *p++ = c; }
  while (j--) *p++ = "xyz 1.-+"[rr (8)];
  *p = 0;
}
static const int bases[] = { 0, 0, 10, 10, 16, 16, 8, 2, 36, 3, 7, 12, 20, 30, 1, 37, -1 };
static void test_numbers (void)
{
  sect_begin ("numbers", 3);
  for (unsigned i = 0; i < N (800); i++)
    {
      char d[64], *e = 0;
      int base = bases[rr (sizeof bases / sizeof bases[0])];
      gen_num (d, i & 1 ? 5 : 22);
      case_begin (i);
      rec_s (d); rec_i (base);
      T_ERRNO = 0; e = 0; long long ll = strtoll (d, &e, base); rec_i (ll); rec_i (epos (d, e)); rec_i (norm_err ());
      T_ERRNO = 0; e = 0; unsigned long long ull = strtoull (d, &e, base); rec_u (ull); rec_i (epos (d, e)); rec_i (norm_err ());
      if (i & 1)                                                       /* few digits: the same answer for a 32 bit and a 64 bit long */
	{
	  T_ERRNO = 0; e = 0; long l = strtol (d, &e, base); rec_i (l); rec_i (epos (d, e)); rec_i (norm_err ());
	  T_ERRNO = 0; e = 0; unsigned long ul = strtoul (d, &e, base); rec_u ((unsigned) ul); rec_i (epos (d, e)); rec_i (norm_err ());
	  if (T_ERRNO == 0 && rr (3) == 0) { rec_i (atoi (d)); rec_i (atol (d)); rec_i (atoll (d)); }
	}
      case_end ();
    }
  sect_end ();
}

/* ======================================================================== qsort, bsearch, div, abs */
static int cmp_int (const void *a, const void *b) { int x = *(const int *) a, y = *(const int *) b; return (x > y) - (x < y); }
typedef struct { int key, tag; } rec2;
static int cmp_rec (const void *a, const void *b) { return cmp_int (a, b); }
static int cmp_bytes (const void *a, const void *b)
{
  const unsigned char *x = a, *y = b;
  for (int i = 0; i < 7; i++) if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
  return 0;
}
static void test_sort (void)
{
  sect_begin ("sort", 4);
  for (unsigned i = 0; i < N (300); i++)
    {
      int arr[70], key;
      unsigned n = rr (66), range = rr (3) == 0 ? 1000000 : 12;
      case_begin (i);
      for (unsigned k = 0; k < n; k++) arr[k] = (int) rr (range) - (int) range / 2;
      qsort (arr, n, sizeof arr[0], cmp_int);
      rec_i (n); rec_m (arr, n * sizeof arr[0]);
      for (int q = 0; q < 6; q++)
	{
	  key = (int) rr (range + 4) - (int) range / 2 - 2;
	  int *f = bsearch (&key, arr, n, sizeof arr[0], cmp_int);
	  rec_i (key); rec_i (f ? f - arr : -1);
	}
      { rec2 r[70]; long sum = 0; int ok = 1;                         /* equal keys may come out in any order: check the order and the contents instead */
	for (unsigned k = 0; k < n; k++) { r[k].key = (int) rr (10); r[k].tag = (int) k; sum += (long) k * 3 + 1; }
	qsort (r, n, sizeof r[0], cmp_rec);
	long sum2 = 0; for (unsigned k = 0; k < n; k++) { sum2 += (long) r[k].tag * 3 + 1; if (k && r[k - 1].key > r[k].key) ok = 0; }
	check (ok && sum == sum2, "qsort of records"); }
      { unsigned char bb[60 * 7]; unsigned m = rr (60);                /* odd element size */
	for (unsigned k = 0; k < m * 7; k++) bb[k] = (unsigned char) rr (4);
	qsort (bb, m, 7, cmp_bytes);
	rec_m (bb, m * 7); }
      case_end ();
    }
  sect_end ();
}
static void test_div (void)
{
  sect_begin ("div", 5);
  for (unsigned i = 0; i < N (500); i++)
    {
      case_begin (i);
      int n = rshift (24), d = rshift (23) >> 8;
      if (d == 0) d = 7;
      long ln = (long) (n / 4) * (long) (rr (4) + 1), ld = d;                        /* no overflow of a 32 bit long */
      long long lln = (long long) n * (long long) (int) rnd (), lld = (long long) d * (long long) (rr (5) + 1);
      div_t a = div (n, d); rec_i (a.quot); rec_i (a.rem);
      ldiv_t b = ldiv (ln, ld); rec_i (b.quot); rec_i (b.rem);
      lldiv_t c = lldiv (lln, lld); rec_i (c.quot); rec_i (c.rem);
      rec_i (abs (n)); rec_i (labs (ln)); rec_i (llabs (lln));
      case_end ();
    }
  sect_end ();
}

/* ======================================================================== sscanf */
enum { K_NONE, K_INT, K_SHORT, K_CHAR8, K_LL, K_STR, K_CH1, K_N };
typedef struct { const char *fmt; int kind; } cv_t;
static const cv_t convs[] = {
  { "%d", K_INT }, { "%i", K_INT }, { "%u", K_INT }, { "%x", K_INT }, { "%X", K_INT }, { "%o", K_INT },
  { "%hd", K_SHORT }, { "%hu", K_SHORT }, { "%hhd", K_CHAR8 }, { "%lld", K_LL }, { "%llx", K_LL },
  { "%2d", K_INT }, { "%3x", K_INT }, { "%5i", K_INT }, { "%c", K_CH1 }, { "%s", K_STR }, { "%4s", K_STR },
  { "%[abc]", K_STR }, { "%[^,]", K_STR }, { "%[a-z]", K_STR }, { "%2[0-9]", K_STR }, { "%[]a]", K_STR },
};
static const char *const supp[] = { "%*d", "%*s", "%*c", "%*[a-c]", "%*x" };
static const char *const lits[] = { " ", ",", ":", "a", "-", "x", "\t ", "  ", "ab", "=", "%%", "b" };
static void gen_scan_input (char *d)
{
  char *p = d;
  unsigned nt = rr (6);
  while (nt--)
    {
      unsigned k = rr (9);
      if (k <= 1) { unsigned nd = 1 + rr (11); if (rr (3) == 0) *p++ = '-'; else if (rr (6) == 0) *p++ = '+'; while (nd--) *p++ = (char) ('0' + rr (10)); }
      else if (k == 2) { unsigned nd = 1 + rr (8); *p++ = '0'; *p++ = rr (4) ? 'x' : 'X'; while (nd--) *p++ = "0123456789abcdefABCDEF"[rr (22)]; }
      else if (k == 3) { unsigned nd = rr (6); *p++ = '0'; while (nd--) *p++ = (char) ('0' + rr (8)); }
      else if (k == 4) { unsigned nw = 1 + rr (6); while (nw--) *p++ = (char) ('a' + rr (6)); }
      else if (k == 5) *p++ = ",:-=%x]"[rr (7)];
      else if (k == 6) { unsigned ns = 1 + rr (3); while (ns--) *p++ = " \t"[rr (2)]; }
    }
  *p = 0;
}
static void test_sscanf (void)
{
  sect_begin ("sscanf", 8);
  for (unsigned i = 0; i < N (2500); i++)
    {
      char fmt[96], in[96];
      unsigned char w[8][64];
      int kinds[8], na = 0, assigned = 0;
      unsigned np = 1 + rr (4);
      char *f = fmt;
      case_begin (i);
      while (np--)
	{
	  unsigned r = rr (11);
	  const char *s;
	  if (r < 5) { const cv_t *c = &convs[rr (sizeof convs / sizeof convs[0])]; s = c->fmt; kinds[na++] = c->kind; assigned++; }
	  else if (r == 5) { s = "%n"; kinds[na++] = K_N; }
	  else if (r < 9) s = lits[rr (sizeof lits / sizeof lits[0])];
	  else if (r == 9 && assigned >= 1) s = supp[rr (sizeof supp / sizeof supp[0])];                 /* glibc answers EOF after only suppressed conversions; the standard 0: not generated */
	  else s = " ";
	  while (*s) *f++ = *s++;
	}
      *f = 0;
      gen_scan_input (in);
      t_set (w, 0xA5, sizeof w);
      int r = sscanf (in, fmt, w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]);
      rec_s (in); rec_s (fmt); rec_i (r);
      for (int j = 0; j < na; j++) rec_m (w[j], kinds[j] == K_STR ? 64 : 8);
      case_end ();
    }
  sect_end ();
}

/* ======================================================================== snprintf */
typedef union { int i; long l; long long ll; intmax_t j; size_t z; ptrdiff_t t; const char *s; void *p; } arg_t;
#define SNP(field) switch (stars) { case 0: return snprintf (out, cap, fmt, a.field); case 1: return snprintf (out, cap, fmt, w, a.field); case 2: return snprintf (out, cap, fmt, p, a.field); \
                                    default: return snprintf (out, cap, fmt, w, p, a.field); }
/* TYPE: 0 int (also h, hh and c), 1 long, 2 long long, 3 intmax_t, 4 size_t, 5 ptrdiff_t, 6 string, 7 pointer */
static int do_snprintf (char *out, size_t cap, const char *fmt, int stars, int type, int w, int p, arg_t a)
{
  switch (type)
    {
    case 0: SNP (i)
    case 1: SNP (l)
    case 2: SNP (ll)
    case 3: SNP (j)
    case 4: SNP (z)
    case 5: SNP (t)
    case 6: SNP (s)
    default: SNP (p)
    }
}
static void test_printf (void)
{
  static const char *const lens[] = { "", "", "", "l", "ll", "h", "hh", "j", "z", "t" };
  sect_begin ("printf", 9);
  for (unsigned i = 0; i < N (3000); i++)
    {
      char fmt[40], out[128], str[24];
      char *f = fmt;
      case_begin (i);
      static const char convc[] = "diuxXocspd";
      char cv = convc[rr (10)];
      int is_int = cv != 'c' && cv != 's' && cv != 'p';
      int signed_conv = cv == 'd' || cv == 'i';
      int stars = 0, w = (int) rr (14), p = (int) rr (8), type = 0;
      arg_t a;
      *f++ = '%';
      /* flags (only those that are defined for the conversion; any order, a repeat is allowed) */
      for (unsigned nf = rr (4); nf; nf--)
        {
          unsigned k = rr (5);
          if (k == 0) *f++ = '-';
          else if (k == 1 && is_int && signed_conv) *f++ = '+';
          else if (k == 2 && is_int && signed_conv) *f++ = ' ';
          else if (k == 3 && (cv == 'x' || cv == 'X' || cv == 'o')) *f++ = '#';
          else if (k == 4 && is_int) *f++ = '0';
        }
      unsigned wr = rr (4);
      if (wr == 1) { *f++ = '*'; stars |= 1; }
      else if (wr == 2) { *f++ = (char) ('1' + rr (9)); if (rr (2)) *f++ = (char) ('0' + rr (10)); }
      if ((cv == 's' || is_int) && rr (2))
        {
          *f++ = '.';
          if (rr (3) == 0) { *f++ = '*'; stars |= 2; }
          else if (rr (4) == 0) { /* just a dot: precision 0 */ }
          else *f++ = (char) ('0' + rr (10));
        }
      const char *len = "";
      if (is_int)
        {
          len = lens[rr (10)];
          while (*len) *f++ = *len++;
          len = f[-1] == 'h' && f[-2] == 'h' ? "hh" : f[-1] == 'h' ? "h" : f[-1] == 'l' && f[-2] == 'l' ? "ll" : f[-1] == 'l' ? "l" : f[-1] == 'j' ? "j" : f[-1] == 'z' ? "z" : f[-1] == 't' ? "t" : "";
        }
      *f++ = cv; *f = 0;
      {
        int v = rshift (28);
        unsigned vh = rnd (), vl = rnd (), sh = rr (62);                                          /* (one statement each: the order of the calls in an expression is not fixed) */
        long long big = (long long) ((unsigned long long) vh << 32 | vl) >> sh;
        if (cv == 's') { rstr (str, sizeof str, 16); a.s = str; type = 6; }
        else if (cv == 'c') { a.i = (int) (32 + rr (95)); }
        else if (cv == 'p') { a.p = rr (4) == 0 ? (void *) 0 : (void *) (uintptr_t) (unsigned) v; type = 7; }
        else if (!strcmp (len, "ll")) { a.ll = big; type = 2; }
        else if (!strcmp (len, "j")) { a.j = (intmax_t) big; type = 3; }
        else if (!strcmp (len, "l")) { if (signed_conv) a.l = (long) v; else a.l = (long) (unsigned) v; type = 1; }
        else if (!strcmp (len, "z")) { a.z = (size_t) (unsigned) v; type = 4; if (signed_conv) a.z = (size_t) (ptrdiff_t) v; }
        else if (!strcmp (len, "t")) { a.t = signed_conv ? (ptrdiff_t) v : (ptrdiff_t) (unsigned) v; type = 5; }
        else a.i = v;                                                                          /* int, h, hh: the whole int is passed, the conversion cuts it */
      }
      if (stars & 1) { w = (int) rr (14); if (rr (4) == 0) w -= 7; }
      if (stars & 2) { p = (int) rr (8); if (rr (5) == 0) p = -1 - (int) rr (3); }
      size_t cap = rr (4) == 0 ? rr (12) : sizeof out;
      t_set (out, 0xA5, sizeof out);
      int r = do_snprintf (out, cap, fmt, stars, type, w, p, a);
      rec_s (fmt); rec_i (w); rec_i (p); rec_i ((long long) cap); rec_i (r); rec_m (out, cap < sizeof out ? cap + 1 : sizeof out);
      case_end ();
    }
  sect_end ();
}

/* ======================================================================== time (the calendar functions against the host's, in UTC) */
static void copy_tm (struct tm *d, const struct tm *s)
{
  t_set (d, 0, sizeof *d);
  d->tm_sec = s->tm_sec; d->tm_min = s->tm_min; d->tm_hour = s->tm_hour; d->tm_mday = s->tm_mday; d->tm_mon = s->tm_mon;
  d->tm_year = s->tm_year; d->tm_wday = s->tm_wday; d->tm_yday = s->tm_yday; d->tm_isdst = s->tm_isdst;
}
static void rec_tm (const struct tm *t)
{
  rec_i (t->tm_sec); rec_i (t->tm_min); rec_i (t->tm_hour); rec_i (t->tm_mday); rec_i (t->tm_mon); rec_i (t->tm_year); rec_i (t->tm_wday); rec_i (t->tm_yday); rec_i (t->tm_isdst);
}
static const char *const tfmts[] = {
  "%a %A %b %B", "%c", "%C %d %D %e", "%F %g %G %h", "%H %I %j %k %l", "%m %M %n %p %P", "%r %R %s", "%S %t %T %u %U", "%V %w %W %x %X", "%y %Y %z %Z %%",
  "%Ec %Ex %EX %Ey %EY %Od %Oe %OH", "%q|%", "x%Hy", "", "%Y-%m-%dT%H:%M:%S", "%A, %d %B %Y", "%U %W %V %G", "%e%e%e",
};
static void test_time (void)
{
  sect_begin ("time", 7);
  for (unsigned i = 0; i < N (500); i++)
    {
      struct tm tm, tm2;
      char out[200];
      long long t = (long long) (rnd () % 4122167296u) - 2061083648LL;           /* a 32 bit time with a margin of 1000 days for what mktime is given below */
      time_t tt = (time_t) t;
      case_begin (i);
      rec_i (t);
      copy_tm (&tm, gmtime (&tt)); rec_tm (&tm);
      copy_tm (&tm2, localtime (&tt)); rec_tm (&tm2);
      copy_tm (&tm2, &tm);
      tm2.tm_mday += (int) rr (70) - 35; tm2.tm_hour += (int) rr (50) - 25; tm2.tm_min += (int) rr (130) - 65; tm2.tm_sec += (int) rr (200) - 100;
      tm2.tm_mon += (int) rr (30) - 15; tm2.tm_year += (int) rr (3) - 1;
      { time_t r = mktime (&tm2); rec_i ((long long) r); rec_tm (&tm2); }
      rec_s (asctime (&tm)); rec_s (ctime (&tt));
      for (unsigned q = 0; q < 3; q++)
	{
	  const char *fm = tfmts[rr (sizeof tfmts / sizeof tfmts[0])];
	  size_t cap = rr (3) == 0 ? rr (9) : 160, n;
	  t_set (out, 0x5A, sizeof out);
	  n = strftime (out, cap, fm, &tm);
	  rec_s (fm); rec_i ((long long) cap); rec_i ((long long) n); if (n) rec_s (out);
	}
      case_end ();
    }
  sect_end ();
}

/* ======================================================================== limits and the like (answers worked out here, not taken from glibc) */
#if !defined (T_ORACLE)
static const int rand1[] = { 551763795, 1262442611, 331412042, 1647693214 }, rand2[] = { 315399501, 261785573, 456450516, 810211168 };
#endif
static void test_limits (void)
{
  char *e;
  sect_begin ("limits", 10);
#if LONG_MAX == 0x7fffffffL
  T_ERRNO = 0; check (strtol ("2147483647", &e, 10) == 2147483647L && T_ERRNO == 0 && *e == 0, "strtol 2^31 - 1");
  T_ERRNO = 0; check (strtol ("2147483648", &e, 10) == LONG_MAX && T_ERRNO == ERANGE && *e == 0, "strtol 2^31 is out of range");
  T_ERRNO = 0; check (strtol ("-2147483648", &e, 10) == LONG_MIN && T_ERRNO == 0, "strtol -2^31");
  T_ERRNO = 0; check (strtol ("-2147483649", &e, 10) == LONG_MIN && T_ERRNO == ERANGE, "strtol -2^31 - 1 is out of range");
  T_ERRNO = 0; check (strtoul ("4294967295", &e, 10) == ULONG_MAX && T_ERRNO == 0, "strtoul 2^32 - 1");
  T_ERRNO = 0; check (strtoul ("4294967296", &e, 10) == ULONG_MAX && T_ERRNO == ERANGE, "strtoul 2^32 is out of range");
  T_ERRNO = 0; check (strtoul ("-1", &e, 10) == ULONG_MAX && T_ERRNO == 0, "strtoul -1");
  T_ERRNO = 0; check (strtoul ("0xFFFFFFFF", &e, 0) == ULONG_MAX && T_ERRNO == 0, "strtoul 0xFFFFFFFF");
  check (atoi ("2147483647") == 2147483647, "atoi INT_MAX");
#else
  T_ERRNO = 0; check (strtol ("9223372036854775807", &e, 10) == LONG_MAX && T_ERRNO == 0, "strtol LONG_MAX");
  T_ERRNO = 0; check (strtol ("9223372036854775808", &e, 10) == LONG_MAX && T_ERRNO == ERANGE, "strtol LONG_MAX + 1");
#endif
  T_ERRNO = 0; check (strtoll ("9223372036854775807", &e, 10) == LLONG_MAX && T_ERRNO == 0, "strtoll LLONG_MAX");
  T_ERRNO = 0; check (strtoll ("9223372036854775808", &e, 10) == LLONG_MAX && T_ERRNO == ERANGE, "strtoll LLONG_MAX + 1");
  T_ERRNO = 0; check (strtoll ("-9223372036854775808", &e, 10) == LLONG_MIN && T_ERRNO == 0, "strtoll LLONG_MIN");
  T_ERRNO = 0; check (strtoll ("-9223372036854775809", &e, 10) == LLONG_MIN && T_ERRNO == ERANGE, "strtoll LLONG_MIN - 1");
  T_ERRNO = 0; check (strtoull ("18446744073709551615", &e, 10) == ULLONG_MAX && T_ERRNO == 0, "strtoull ULLONG_MAX");
  T_ERRNO = 0; check (strtoull ("18446744073709551616", &e, 10) == ULLONG_MAX && T_ERRNO == ERANGE, "strtoull ULLONG_MAX + 1");
  T_ERRNO = 0; check (strtoull ("-1", &e, 10) == ULLONG_MAX && T_ERRNO == 0, "strtoull -1");
  T_ERRNO = 0; check (strtoull ("0xFFFFFFFFFFFFFFFF", &e, 16) == ULLONG_MAX && T_ERRNO == 0, "strtoull 0xFFFFFFFFFFFFFFFF");
  T_ERRNO = 0; check (strtoull ("340282366920938463463374607431768211456", &e, 10) == ULLONG_MAX && T_ERRNO == ERANGE, "strtoull 2^128");
  { const char *s = "0xg"; check (strtol (s, &e, 0) == 0 && e == s + 1, "strtol 0x without digits"); }
  { const char *s = "  12abc"; check (strtol (s, &e, 10) == 12 && e == s + 4, "strtol stops at a letter"); }
  { const char *s = "abc"; check (strtol (s, &e, 10) == 0 && e == s, "strtol without digits"); }
  { const char *s = " - 5"; check (strtol (s, &e, 10) == 0 && e == s, "strtol sign then blank"); }
  { const char *s = "0x1f"; check (strtol (s, &e, 16) == 31 && *e == 0, "strtol 0x1f with base 16"); }
  { const char *s = "017"; check (strtol (s, &e, 0) == 15 && *e == 0, "strtol 017 is octal"); }
  { const char *s = "zz"; check (strtol (s, &e, 36) == 35 * 36 + 35 && *e == 0, "strtol zz base 36"); }
  T_ERRNO = 0; check (strtol ("5", 0, 1) == 0 && T_ERRNO == EINVAL, "strtol base 1 is invalid");
  check (setlocale (LC_ALL, 0) != 0 && setlocale (LC_ALL, "C") != 0 && setlocale (LC_ALL, "xx_YY") == 0, "setlocale");
  check (localeconv ()->decimal_point[0] == '.' && localeconv ()->decimal_point[1] == 0 && localeconv ()->grouping[0] == 0, "localeconv");
  sect_end ();
}
#if !defined (T_ORACLE)
static void test_rand (void)
{
  sect_begin ("rand", 14);
  srand (1); for (int k = 0; k < 4; k++) check (rand () == rand1[k], "rand after srand (1)");
  srand (0x12345); for (int k = 0; k < 4; k++) check (rand () == rand2[k], "rand after srand (0x12345)");
  srand (7); { int ok = 1; for (int k = 0; k < 2000; k++) { int r = rand (); if (r < 0 || r > RAND_MAX) ok = 0; } check (ok, "rand stays in 0 .. RAND_MAX"); }
  sect_end ();
}
#endif

#if defined (T_HOSTLIB) || (defined (T_ARM) && !defined (T_HW))
/* ======================================================================== _swi / _swix (the model of the test SWIs is in hosthooks.c for the host and in armrun.py for the interpreter) */
#include <kernel.h>
#include <swis.h>
#define TST_SWI		0x5AB00
#define TST_SETCLOCK	0x5AB01
#define TST_SETMONO	0x5AB02
#define TST_BLOCK	0x5AB03
static void test_swix (void)
{
  unsigned o[10], fl = 0;
  _kernel_oserror *e;
  sect_begin ("swix", 11);
  case_begin (0);
  for (int k = 0; k < 10; k++) o[k] = 0x55;
  e = _swix (TST_SWI, _INR (0, 9) | _OUTR (0, 9) | _OUT (_FLAGS), 5u, 7u, 0x100u, 3u, 100u, 40u, 0xF0u, 0x0Fu, 77u, 88u, &o[0], &o[1], &o[2], &o[3], &o[4], &o[5], &o[6], &o[7], &o[8], &o[9], &fl);
  rec_i (e != 0); rec_m (o, sizeof o); rec_u (fl); case_end ();
  case_begin (1);
  for (int k = 0; k < 10; k++) o[k] = 0x55;
  e = _swix (TST_SWI, _IN (0) | _IN (1) | _OUT (0), 9u, 4u, &o[0]);
  rec_i (e != 0); rec_m (o, sizeof o); case_end ();
  case_begin (2);
  for (int k = 0; k < 10; k++) o[k] = 0x55;
  e = _swix (TST_SWI, _INR (2, 5) | _OUT (1) | _OUT (4), 1u, 2u, 3u, 4u, &o[1], &o[4]);
  rec_i (e != 0); rec_m (o, sizeof o); case_end ();
  case_begin (3);
  fl = 0x55;
  e = _swix (TST_SWI, _INR (0, 1) | _OUT (_FLAGS), 2u, 3u, &fl);
  rec_i (e != 0); rec_u (fl); case_end ();
  case_begin (4);
  for (int k = 0; k < 10; k++) o[k] = 0x55;
  e = _swix (TST_SWI, _INR (0, 1) | _OUT (0), 1u, 0xBADu, &o[0]);                 /* the SWI gives an error: no output is stored */
  rec_i (e != 0); rec_m (o, sizeof o);
  if (e) { rec_i (e->errnum); rec_s (e->errmess); }
  { _kernel_oserror *l = _kernel_last_oserror (); rec_i (l != 0); if (l) { rec_i (l->errnum); rec_s (l->errmess); } rec_i (_kernel_last_oserror () != 0); }
  case_end ();
  case_begin (5);
  rec_i (_swi (TST_SWI | 0x20000, _INR (0, 1) | _RETURN (1), 3u, 4u));                           /* r2 ^ 0x1234 */
  rec_u ((unsigned) _swi (TST_SWI | 0x20000, _INR (0, 1) | _RETURN (_FLAGS), 3u, 4u));        /* the flags */
  rec_i (_swi (TST_SWI | 0x20000, _INR (0, 1) | _RETURN (0), 10u, 20u));
  rec_i (_swi (TST_SWI | 0x20000, _INR (0, 1), 10u, 21u));                                        /* no _RETURN: r0 */
  rec_i (_swi (TST_SWI | 0x20000, _INR (0, 3) | _OUT (2) | _RETURN (3), 1u, 2u, 5u, 9u, &o[2]));
  rec_u (o[2]);
  case_end ();
  sect_end ();
}
#if defined (T_ARM)
static void test_swix_block (void)                                                              /* _BLOCK needs the ARM argument layout */
{
  unsigned o[10] = { 0 };
  _kernel_oserror *e;
  sect_begin ("swixblock", 15);
  case_begin (0);
  e = _swix (TST_BLOCK, _IN (0) | _BLOCK (1) | _OUT (0), 3u, &o[0], 11u, 22u, 33u);           /* r1 points at the block: the words that follow the arguments */
  rec_i (e != 0); rec_u (o[0]);
  e = _swix (TST_BLOCK, _IN (0) | _BLOCK (3) | _OUT (0) | _OUT (3), 2u, &o[0], &o[3], 1000u, 2000u);
  rec_i (e != 0); rec_u (o[0]); rec_u (o[3] != 0);
  case_end ();
  check (o[0] == 3000, "_BLOCK: the sum of the words of the block");
  sect_end ();
}
#endif
static void set_clock (unsigned long long cs) { _swix (TST_SETCLOCK, _INR (0, 1), (unsigned) cs, (unsigned) (cs >> 32)); }
static void test_clock (void)
{
  sect_begin ("clock", 12);
  for (unsigned i = 0; i < N (200); i++)
    {
      unsigned long long cs = (unsigned long long) rr (255) << 32;
      cs |= rnd ();
      time_t t2 = 0, t;
      long long exp;
      if (i == 0) cs = 0;
      if (i == 1) cs = 220898880000ULL;                                                     /* 1 Jan 1970 */
      set_clock (cs);
      t = time (&t2);
      exp = (long long) (cs / 100) - 2208988800LL;
      if (exp < LONG_MIN) exp = LONG_MIN;
      if (exp > LONG_MAX) exp = LONG_MAX;
      check (t == (time_t) exp && t2 == t, "time () from the clock");
      unsigned v = rnd ();
      _swix (TST_SETMONO, _IN (0), v);
      check (clock () == (clock_t) v, "clock ()");
    }
  sect_end ();
}
#endif

#if defined (T_HW)
/* ======================================================================== the real SWIs (the machine, not the interpreter) */
#include <kernel.h>
#include <swis.h>
static void test_hw (void)
{
  _kernel_oserror *e;
  unsigned n = 0xFFFFFFFFu;
  sect_begin ("hw", 18);
  e = _swix (0x39, _IN (1) | _OUT (0), "OS_WriteC", &n);                                    /* OS_SWINumberFromString */
  check (e == 0 && n == 0, "OS_SWINumberFromString OS_WriteC is 0");
  e = _swix (0x39, _IN (1) | _OUT (0), "OS_Byte", &n);
  check (e == 0 && n == 6, "OS_SWINumberFromString OS_Byte is 6");
  e = _swix (0x39, _IN (1) | _OUT (0), "Nonsuch_ZzzQ", &n);
  check (e != 0 && e->errnum == 0x1E6, "an unknown SWI name is the error &1E6");
  { _kernel_oserror *l = _kernel_last_oserror (); check (l != 0 && l->errnum == 0x1E6, "_kernel_last_oserror () has it"); }
  { char buf[16]; char *end = 0;
    e = _swix (0xD8, _INR (0, 2) | _OUT (1), 1234567u, buf, sizeof buf, &end);               /* OS_ConvertCardinal4 */
    check (e == 0 && end == buf + 7 && buf[0] == '1' && buf[6] == '7' && buf[7] == 0, "OS_ConvertCardinal4 with _INR and _OUT (r1 is the end of the text)"); }
  { unsigned t0 = (unsigned) _swi (0x42, _RETURN (0)), t1 = t0;                              /* OS_ReadMonotonicTime and clock () */
    for (unsigned spin = 0; spin < 20000000u && t1 - t0 < 5; spin++) t1 = (unsigned) _swi (0x42, _RETURN (0));
    check (t1 - t0 >= 5 && t1 - t0 < 20, "the monotonic time runs (5 centiseconds pass)");
    unsigned c = (unsigned) clock (); unsigned m = (unsigned) _swi (0x42, _RETURN (0));
    check (m - c < 3, "clock () is the monotonic time"); }
  { time_t a = time (0), b;
    check (a > 1704067200L, "time () is later than 1 Jan 2024");
    for (unsigned spin = 0; spin < 50000000u; spin++) { b = time (0); if (b - a >= 1) break; }
    check (b - a == 1, "time () advances by 1 in a second"); }
  { struct tm *g = gmtime (&(time_t) { 86400 * 365 }); check (g && g->tm_year == 71 && g->tm_mon == 0 && g->tm_mday == 1, "gmtime on the machine"); }
  sect_end ();
}
#endif

#if defined (T_ARM)
#include <setjmp.h>
static jmp_buf jb;
static void jumper (int n) { if (n == 0) longjmp (jb, 42); jumper (n - 1); }
/* malloc / calloc / realloc / free on the memory of the machine (the RMA: OS_Module 6 and 7): random operations on 40 slots, every block filled with a pattern that is checked before the block is changed or
   freed (so that an overlap or a copy that lost bytes shows), alignment, calloc zeroes, the sizes that cannot be given */
static void test_heap (void)
{
  enum { SLOTS = 40 };
  static struct { unsigned char *p; size_t n; unsigned char seed; } s[SLOTS];
  sect_begin ("heap", 17);
  for (unsigned i = 0; i < N (3000); i++)
    {
      unsigned k = rr (SLOTS), op = rr (5);
      size_t n = rr (3) == 0 ? rr (8) : rr (400);
      if (s[k].p)
        {
          int ok = 1;
          for (size_t j = 0; j < s[k].n; j++) if (s[k].p[j] != (unsigned char) (s[k].seed + j)) ok = 0;
          check (ok, "a block keeps its contents");
        }
      if (op == 0 || op == 1)
        {
          free (s[k].p);
          s[k].p = (unsigned char *) malloc (n); s[k].n = s[k].p ? n : 0;
          check (s[k].p != 0 && ((unsigned) s[k].p & 7) == 0, "malloc gives an 8 byte aligned block");
        }
      else if (op == 2)
        {
          unsigned char *q = (unsigned char *) realloc (s[k].p, n);
          check (q != 0 || n == 0, "realloc succeeds");
          if (s[k].p)
            {
              size_t keep = s[k].n < n ? s[k].n : n; int ok = 1;
              for (size_t j = 0; j < keep; j++) if (q[j] != (unsigned char) (s[k].seed + j)) ok = 0;
              check (ok, "realloc keeps the contents");
            }
          s[k].p = q; s[k].n = q ? n : 0;
        }
      else if (op == 3)
        {
          size_t a = rr (10), b = rr (30);
          free (s[k].p);
          s[k].p = (unsigned char *) calloc (a, b); s[k].n = s[k].p ? a * b : 0;
          int ok = s[k].p != 0;
          for (size_t j = 0; ok && j < s[k].n; j++) if (s[k].p[j]) ok = 0;
          check (ok, "calloc gives zeroes");
        }
      else { free (s[k].p); s[k].p = 0; s[k].n = 0; }
      if (s[k].p) { s[k].seed = (unsigned char) rnd (); for (size_t j = 0; j < s[k].n; j++) s[k].p[j] = (unsigned char) (s[k].seed + j); }
      for (unsigned m = 0; m < SLOTS; m++)                                                          /* no two live blocks overlap */
        if (m != k && s[m].p && s[k].p && s[k].n && s[m].n && s[k].p < s[m].p + s[m].n && s[m].p < s[k].p + s[k].n) check (0, "two live blocks overlap");
    }
  for (unsigned m = 0; m < SLOTS; m++) { free (s[m].p); s[m].p = 0; }
  errno = 0; check (malloc (0xFFFFFFF0u) == 0 && errno == ENOMEM, "malloc of 4 GB fails with ENOMEM");
  errno = 0; check (malloc (0x7FFFFFF1u) == 0 && errno == ENOMEM, "malloc of 2 GB fails with ENOMEM");
  errno = 0; check (calloc (0x10000, 0x10000) == 0 && errno == ENOMEM, "calloc whose product wraps fails with ENOMEM");
  free (0);
  sect_end ();
}

static void test_arm_only (void)
{
  volatile int cnt = 0;
  int r;
  sect_begin ("arm", 13);
  r = setjmp (jb);
  if (r == 0) { cnt++; jumper (5); check (0, "longjmp did not jump"); }
  else check (r == 42 && cnt == 1, "setjmp returns the value of longjmp");
  r = setjmp (jb);
  if (r == 0) longjmp (jb, 0);
  else check (r == 1, "longjmp (jb, 0) makes setjmp return 1");
  { char *v = getenv ("Test$Var"); check (v && !strcmp (v, "hello world"), "getenv of a system variable"); check (getenv ("Test$Missing") == 0, "getenv of a variable that is not there"); }
  { _kernel_oserror *er = _kernel_setenv ("Test$Set", "abc"); check (er == 0, "_kernel_setenv"); char *v = getenv ("Test$Set"); check (v && !strcmp (v, "abc"), "getenv after _kernel_setenv"); }
  sect_end ();
}
#endif

#if defined (T_HW)
/* the module that this program is on the machine (libtest-hw.cmhg): runnable, nothing to initialise */
_kernel_oserror *lt_init (const char *tail, int podule_base, void *pw) { (void) tail; (void) podule_base; (void) pw; return 0; }
#endif

/* ======================================================================== main */
#if defined (T_ARM)
int main (void)
#else
int main (int argc, char **argv)
#endif
{
#if !defined (T_ARM)
  for (int i = 1; i < argc; i++) if (argv[i][0] == '-' && argv[i][1] == 'v' && !argv[i][2]) g_verbose = 1;
  setenv ("TZ", "UTC", 1);
  tzset ();
#endif
  test_ctype ();
  test_string ();
  test_numbers ();
  test_sort ();
  test_div ();
  test_sscanf ();
  test_printf ();
  test_time ();
  test_limits ();
#if !defined (T_ORACLE)
  test_rand ();
#endif
#if defined (T_HOSTLIB) || (defined (T_ARM) && !defined (T_HW))
  test_swix ();
  test_clock ();
#if defined (T_ARM)
  test_swix_block ();
#endif
#endif
#if defined (T_ARM)
  test_heap ();
  test_arm_only ();
#endif
#if defined (T_HW)
  test_hw ();
#endif
  { char b[40] = "TOTAL fail="; size_t n = t_len (b); char d[12]; int k = 0; unsigned v = g_fail; do d[k++] = (char) ('0' + v % 10); while (v /= 10); while (k) b[n++] = d[--k]; b[n] = 0; puts (b); }
#if defined (T_HW)
  return 0;                                                                                           /* (a return code that is not 0 is an error of *RMRun: the failures are in the text) */
#else
  return g_fail != 0;
#endif
}
