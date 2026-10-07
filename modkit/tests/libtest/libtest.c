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
static int t_cmp (const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return (unsigned char) *a - (unsigned char) *b; }
static int t_memcmp (const void *a, const void *b, size_t n) { const unsigned char *p = a, *q = b; while (n--) { if (*p != *q) return *p - *q; p++; q++; } return 0; }
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
  if (!ok)
    {
      char b[220] = "FAIL ", d[12];
      size_t n;
      int k = 0, e = T_ERRNO;
      unsigned v = e < 0 ? (unsigned) -e : (unsigned) e;
      g_fail++;
      t_cpy (b + 5, g_sect); n = t_len (b); b[n++] = ' '; t_cpy (b + n, what); n = t_len (b);
      t_cpy (b + n, "  [errno "); n += 9;                                  /* (errno as it is when the check fails: it shows which error a failed call had) */
      if (e < 0) b[n++] = '-';
      do d[k++] = (char) ('0' + v % 10); while (v /= 10);
      while (k) b[n++] = d[--k];
      b[n++] = ']'; b[n] = 0;
      puts (b);
    }
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

/* ======================================================================== stdio: files (against glibc's, on files of the folder T_FSDIR; on the machine of the scrap directory) */
#ifndef T_FSDIR
# define T_FSDIR "/tmp/mkstdio-"
#endif
static char g_fsbase[200];                                                 /* the start of the name of every file of the test */
static const char *fsname (int i)
{
  static char n[4][220];
  size_t l = t_len (g_fsbase);
  t_mov (n[i], g_fsbase, l);
  n[i][l] = 'f'; n[i][l + 1] = (char) ('0' + i); n[i][l + 2] = 0;
  return n[i];
}
/* random bytes with a lot of white space, digits and letters (so that fgets and fscanf have something to read) */
static void rtext (unsigned char *d, unsigned n)
{
  for (unsigned i = 0; i < n; i++)
    {
      unsigned k = rr (10);
      d[i] = (unsigned char) (k == 0 ? '\n' : k == 1 ? ' ' : k < 4 ? '0' + rr (10) : k < 7 ? 'a' + rr (26) : k == 7 ? (rr (2) ? '-' : '+') : k == 8 ? (int) rr (256) : (int) ('A' + rr (26)));
    }
}
static void put_file (const char *name, unsigned n)                         /* a file of N random bytes, made with fopen / fwrite */
{
  unsigned char b[1400];
  FILE *f = fopen (name, "wb");
  rtext (b, n);
  if (f) { if (n) fwrite (b, 1, n, f); fclose (f); }
}
static void rec_file (const char *name)                                    /* the whole file */
{
  unsigned char b[100];
  FILE *f = fopen (name, "rb");
  rec_i (f != 0);
  if (!f) return;
  for (;;)
    {
      size_t k = fread (b, 1, sizeof b, f);
      rec_i ((long long) k);
      if (k) rec_m (b, k);
      if (k < sizeof b) break;
    }
  rec_i (ferror (f) != 0);
  fclose (f);
}
static void test_stdio (void)
{
  static const char *const modes[] = { "r", "w", "a", "r+", "w+", "a+", "rb", "wb", "ab", "r+b", "w+b", "a+b", "rb+", "wb+" };
  const char *name = fsname (0), *name2 = fsname (1);
  sect_begin ("stdio", 19);
  for (unsigned i = 0; i < N (700); i++)
    {
      const char *mode = modes[rr (14)];
      int update = 0, last = 0, lastget = -2;                              /* last: 1 = the last operation read, 2 = it wrote; lastget: the character that the last operation (fgetc) gave */
      unsigned nops;
      FILE *f;
      fpos_t pos;
      int have_pos = 0;
      case_begin (i);
      for (const char *m = mode; *m; m++) if (*m == '+') update = 1;
      rec_s (mode);
      remove (name);
      if (rr (4) != 0) put_file (name, rr (3) == 0 ? rr (30) : rr (1300));
      f = fopen (name, mode);
      rec_i (f != 0);
      if (!f) { rec_i (remove (name) != 0); case_end (); continue; }
      switch (rr (8))
        {
        case 0: rec_i (setvbuf (f, 0, _IONBF, 0)); break;
        case 1: rec_i (setvbuf (f, 0, _IOLBF, 80)); break;
        case 2: rec_i (setvbuf (f, 0, _IOFBF, 100)); break;
        case 3: setbuf (f, 0); break;
        default: break;
        }
      nops = 1 + rr (14);
      for (unsigned k = 0; k < nops; k++)
        {
          unsigned op = rr (17);
          int prev_get = lastget;
          lastget = -2;
          rec_i (-1000 - (long long) op);                                       /* (which operation: for reading a difference in the verbose output) */
          int is_read = op == 3 || op == 4 || op == 5 || op == 6 || op == 14, is_write = op == 0 || op == 1 || op == 2 || op == 13;
          if (update && ((is_read && last == 2) || (is_write && last == 1)))             /* in an update stream a read and a write need a positioning call or a flush between them */
            {
              if (rr (2)) rec_i (fflush (f)); else rec_i (fseek (f, 0, SEEK_CUR));
              last = 0;
            }
          switch (op)
            {
            case 0:                                                        /* fwrite */
              {
                unsigned char b[1400];
                static const unsigned sizes[] = { 1, 1, 2, 3, 7 };
                size_t sz = sizes[rr (5)], cnt = rr (3) == 0 ? rr (200) : rr (40);
                rtext (b, (unsigned) (sz * cnt));
                rec_i ((long long) fwrite (b, sz, cnt, f));
                break;
              }
            case 1: rec_i (fputc ((int) rr (256), f)); break;
            case 2:
              {
                char s[50];
                unsigned len = rr (45);
                rtext ((unsigned char *) s, len);
                for (unsigned j = 0; j < len; j++) if (!s[j]) s[j] = 'z';        /* no NUL inside the string */
                s[len] = 0;
                rec_i (fputs (s, f) >= 0);
                break;
              }
            case 3:                                                        /* fread */
              {
                unsigned char b[1800];
                static const unsigned sizes[] = { 1, 1, 2, 3, 7 };
                size_t sz = sizes[rr (5)], cnt = rr (4) == 0 ? rr (250) : rr (30), got;
                t_set (b, 0xA5, sizeof b);
                got = fread (b, sz, cnt, f);
                rec_i ((long long) got);
                rec_m (b, got * sz);
                break;
              }
            case 4: lastget = fgetc (f); rec_i (lastget); break;
            case 5:
              {
                char s[90];
                int n = (int) rr (75);
                char *r;
                if (n == 1) n = 2;                                         /* (glibc's fgets with 1 is NULL when it is fortified, and the string when it is not) */
                t_set (s, 0x55, sizeof s);
                r = fgets (s, n, f);
                rec_i (n);
                rec_i (r != 0);
                if (r) rec_s (s);
                break;
              }
            case 6: rec_i (ungetc (prev_get >= 0 ? prev_get : EOF, f)); break;                 /* the character that was just read (glibc has trouble with other ones and with many pushed back) */
            case 7:
              {
                static const int whence[] = { SEEK_SET, SEEK_CUR, SEEK_END, SEEK_SET, SEEK_CUR };
                long off = (long) rr (500);
                int w;
                off -= (long) rr (150);
                w = whence[rr (5)];
                if (rr (30) == 0) w = 7;                                    /* not a place */
                rec_i (fseek (f, off, w));
                last = 0;
                break;
              }
            case 8: rec_i (ftell (f)); break;
            case 9: rewind (f); last = 0; break;
            case 10: rec_i (feof (f) != 0); rec_i (ferror (f) != 0); break;
            case 11: clearerr (f); break;
            case 12: rec_i (fflush (f)); last = 0; break;
            case 13:
              {
                char s[40];
                unsigned len = rr (20);
                for (unsigned j = 0; j < len; j++) s[j] = (char) ('a' + rr (26));
                s[len] = 0;
                int num = rshift (20);
                unsigned un = rnd () >> 20;
                rec_i (fprintf (f, "%d:%s|%5.2s|%-4u|", num, s, s, un));
                break;
              }
            case 14:
              {
                int v = -77, r;
                char s[30];
                t_set (s, 0x55, sizeof s);
                s[sizeof s - 1] = 0;
                r = fscanf (f, rr (2) ? "%d %20s" : " %d%20[a-z]", &v, s);
                rec_i (r); rec_i (v); rec_s (s);
                break;
              }
            case 15:
              if (!have_pos) { have_pos = fgetpos (f, &pos) == 0; rec_i (have_pos); }
              else { rec_i (fsetpos (f, &pos)); have_pos = 0; last = 0; }
              break;
            default: rec_i (ftell (f)); break;
            }
          if (is_read) last = 1;
          if (is_write) last = 2;
        }
      rec_i (fclose (f));
      rec_file (name);
      rec_i (remove (name));
      rec_i (remove (name));                                               /* the second one: not there */
      case_end ();
    }
  /* names: rename, remove, a file that is not there */
  for (unsigned i = 0; i < N (20); i++)
    {
      unsigned char b[64];
      FILE *f;
      case_begin (i);
      remove (name); remove (name2);
      put_file (name, rr (60));
      rec_i (rename (name, name2));
      rec_i (rename (name, name2));                                        /* the old one is gone */
      f = fopen (name, "rb"); rec_i (f != 0);
      f = fopen (name2, "rb"); rec_i (f != 0);
      if (f) { size_t k = fread (b, 1, sizeof b, f); rec_i ((long long) k); rec_m (b, k); fclose (f); }
      rec_i (remove (name2));
      rec_i (remove (name2));
      case_end ();
    }
  sect_end ();
}

#if !defined (T_ORACLE)
/* ======================================================================== stdio: what has answers of its own (errno, freopen, big buffers, the screen and the keyboard streams) */
#include <kernel.h>
#if defined (T_HW)
# define FS_SEP '.'                                                        /* the separator of the path names of the machine, of the host */
#else
# define FS_SEP '/'
#endif
#if defined (T_HOSTLIB)                                                    /* (the library has the BSD / UnixLib numbers; the host's headers are glibc's: the two that differ) */
# undef ENOTEMPTY
# undef EOVERFLOW
# define ENOTEMPTY 66
# define EOVERFLOW 91
#endif
extern int __modlib_closeall (void);
extern void (*__modlib_stdio_end_hook) (int);
#if defined (T_HOSTLIB)
extern void mk_host_fs_ctl (int reason, const char *name, unsigned value);
static void t_fs (int reason, const char *name, unsigned value) { mk_host_fs_ctl (reason, name, value); }
#elif !defined (T_HW)
#include <swis.h>
static void t_fs (int reason, const char *name, unsigned value) { _swix (0x5AB07, _INR (0, 2), (unsigned) reason, name, value); }       /* the faults of the file model, the host's sparse files */
#endif
static int set_attr (const char *name, int attr)                          /* OS_File 4: the attributes (bit 0 read, bit 1 write, bit 3 locked) */
{
  _kernel_osfile_block b;
  b.load = b.exec = b.start = 0; b.end = attr;
  return _kernel_osfile (4, name, &b) != _kernel_ERROR;
}
static long length_of (const char *name)                                   /* OS_File 17: the length that the catalogue has, or -1 */
{
  _kernel_osfile_block b;
  b.load = b.exec = b.start = b.end = 0;
  return _kernel_osfile (17, name, &b) == 1 ? (long) (unsigned) b.start : -1L;
}
static void put_text (const char *name, const char *text)                 /* the file holds TEXT */
{
  FILE *f = fopen (name, "wb");
  if (f) { fputs (text, f); fclose (f); }
}
static int file_is (const char *name, const char *text)                   /* the file holds exactly TEXT */
{
  char b[64];
  size_t n, l = t_len (text);
  FILE *f = fopen (name, "rb");
  if (!f) return 0;
  n = fread (b, 1, sizeof b, f);
  fclose (f);
  return n == l && !t_memcmp (b, text, l);
}
/* the second half of the checks: ungetc and positions, the modes, access, files that are open, errors of the disc, the end of a program, printf details */
static void stdio_more (const char *name, const char *name2, const char *name3)
{
  FILE *f, *g;
  fpos_t pos;
  char b[80];
  int v = -1, v2 = -1;
  /* ungetc: a character other than the one read does not change the file's bytes in the buffer; the position is one back, also after a seek; one character only */
  put_text (name, "ABCDEFGHIJ");
  f = fopen (name, "rb");
  check (f && fgetc (f) == 'A' && ungetc ('Z', f) == 'Z' && fgetc (f) == 'Z' && fseek (f, 0, SEEK_SET) == 0 && fgetc (f) == 'A', "ungetc of another character, then a seek back: the file's byte, not the pushed one");
  check (f && fgetc (f) == 'B' && ungetc ('Q', f) == 'Q' && fseek (f, 1, SEEK_SET) == 0 && fgetc (f) == 'B', "ungetc of another character, a seek inside the buffer: the file's byte");
  check (f && fseek (f, 5, SEEK_SET) == 0 && fgetc (f) == 'F' && ungetc ('x', f) == 'x' && ftell (f) == 5 && fseek (f, 0, SEEK_CUR) == 0 && fgetc (f) == 'F', "ungetc after a read: the position is one back, a seek to it drops the character");
  check (f && fseek (f, 5, SEEK_SET) == 0 && ungetc ('x', f) == 'x' && ftell (f) == 4 && ungetc ('y', f) == EOF && fseek (f, 0, SEEK_CUR) == 0 && fgetc (f) == 'E', "ungetc on an idle stream: ftell is one back, the second one is refused, a seek to the position drops it");
  check (f && fseek (f, 8, SEEK_SET) == 0 && ungetc ('y', f) == 'y' && fgetpos (f, &pos) == 0 && fsetpos (f, &pos) == 0 && fgetc (f) == 'H', "ungetc, fgetpos and fsetpos: the position is the one before the pushed character");
  check (f && fseek (f, 2, SEEK_SET) == 0 && ungetc ('x', f) == 'x' && fseek (f, 1, SEEK_CUR) == 0 && fgetc (f) == 'C', "ungetc then a relative seek counts from the position that the pushed character gives");
  if (f) fclose (f);
  /* fscanf: "0x" with no hex digit is used up, as in glibc; with a small buffer too */
  put_text (name, "0xg rest");
  f = fopen (name, "rb");
  check (f && fscanf (f, "%i", &v) == 0 && ftell (f) == 2 && fgetc (f) == 'g', "fscanf of 0x with no hex digit: 0x is used up, the next character is there");
  if (f) fclose (f);
  f = fopen (name, "rb");
  check (f && setvbuf (f, 0, _IONBF, 0) == 0 && fscanf (f, "%x", &v) == 0 && ftell (f) == 2 && fgetc (f) == 'g', "the same on an unbuffered stream");
  if (f) fclose (f);
  put_text (name, "0x1F, 42 z");
  f = fopen (name, "rb");
  check (f && setvbuf (f, 0, _IONBF, 0) == 0 && fscanf (f, "%i%d", &v, &v2) == 1 && v == 31 && fgetc (f) == ',', "fscanf %i of 0x1F on an unbuffered stream: the comma is the next character");
  if (f) fclose (f);
  /* "wx" */
  put_text (name, "KEEP");
  T_ERRNO = 0; check (fopen (name, "wx") == 0 && T_ERRNO == EEXIST && file_is (name, "KEEP"), "wx of a file that is there: NULL, EEXIST, the file is not touched");
  remove (name);
  f = fopen (name, "wx");
  check (f && fputs ("new", f) >= 0 && fclose (f) == 0 && file_is (name, "new"), "wx of a file that is not there: it is made");
  T_ERRNO = 0; check (fopen (name, "rx") == 0 && T_ERRNO == EINVAL, "x goes with w only: EINVAL");
  /* a file that is read only or locked cannot be opened for writing; nothing of it is touched */
  remove (name2);                                                         /* (FileSwitch looks for the destination before it looks at the source: a rename onto a file is "Bad rename" whatever the state of the source) */
  put_text (name, "RDONLY");
  check (set_attr (name, 0x01), "OS_File 4 makes the file read only");
  T_ERRNO = 0; check (fopen (name, "r+") == 0 && T_ERRNO == EACCES, "r+ of a read only file: NULL and EACCES");
  T_ERRNO = 0; check (fopen (name, "w") == 0 && T_ERRNO == EACCES, "w of a read only file: NULL and EACCES");
  T_ERRNO = 0; check (fopen (name, "a") == 0 && T_ERRNO == EACCES, "a of a read only file: NULL and EACCES");
  T_ERRNO = 0; check (fopen (name, "a+") == 0 && T_ERRNO == EACCES, "a+ of a read only file: NULL and EACCES");
  f = fopen (name, "r");
  check (f && fgets (b, sizeof b, f) && !t_cmp (b, "RDONLY"), "r of a read only file works, and the data is still there");
  if (f) fclose (f);
  check (set_attr (name, 0x0B), "OS_File 4 locks the file");
  T_ERRNO = 0; check (fopen (name, "r+") == 0 && T_ERRNO == EACCES, "r+ of a locked file: NULL and EACCES");
  T_ERRNO = 0; check (remove (name) == -1 && T_ERRNO == EACCES, "remove of a locked file: -1 and EACCES");
  T_ERRNO = 0; check (rename (name, name2) == -1 && T_ERRNO == EACCES, "rename of a locked file: -1 and EACCES");
  check (set_attr (name, 0x03) && file_is (name, "RDONLY"), "writable again, with its data");
  /* a file that is open for writing is open for nobody; two readers are fine; a rename does not replace */
  remove (name2);
  f = fopen (name, "w");
  T_ERRNO = 0; check (f && fopen (name, "r") == 0 && T_ERRNO == EBUSY, "a file open for writing cannot be opened for reading: EBUSY");
  T_ERRNO = 0; check (fopen (name, "w") == 0 && T_ERRNO == EBUSY, "nor for writing: EBUSY");
  T_ERRNO = 0; check (remove (name) == -1 && T_ERRNO == EBUSY, "remove of an open file: -1 and EBUSY");
  T_ERRNO = 0; check (rename (name, name2) == -1 && T_ERRNO == EBUSY, "rename of an open file: -1 and EBUSY");
  if (f) fclose (f);
  put_text (name, "two readers");
  f = fopen (name, "r"); g = fopen (name, "r");
  check (f && g && fgetc (f) == 't' && fgetc (g) == 't', "two streams can read one file");
  T_ERRNO = 0; check (fopen (name, "r+") == 0 && T_ERRNO == EBUSY, "r+ of a file that is open for reading: EBUSY");
  if (f) fclose (f);
  if (g) fclose (g);
  put_text (name2, "the other");
  T_ERRNO = 0; check (rename (name, name2) == -1 && T_ERRNO == EEXIST && file_is (name2, "the other"), "rename onto a file that is there: -1, EEXIST, the file is not replaced");
  f = fopen (name, "r");
  T_ERRNO = 0; check (f && rename (name, name2) == -1 && T_ERRNO == EEXIST, "rename of an open file onto a file that is there: EEXIST (the destination is looked at first)");
  if (f) fclose (f);
  check (set_attr (name, 0x0B), "OS_File 4 locks the file again");
  T_ERRNO = 0; check (rename (name, name2) == -1 && T_ERRNO == EEXIST, "rename of a locked file onto a file that is there: EEXIST (the destination is looked at first)");
  check (set_attr (name, 0x03), "writable again");
  remove (name2);
  /* a folder is not a file */
  {
    char dir[220], inner[230];
    size_t l = t_len (g_fsbase);
    _kernel_osfile_block b = { 0, 0, 0, 0 };
    t_mov (dir, g_fsbase, l); dir[l] = 'd'; dir[l + 1] = 0;
    t_mov (inner, dir, l + 1); inner[l + 1] = FS_SEP; inner[l + 2] = 'x'; inner[l + 3] = 0;
    check (_kernel_osfile (8, dir, &b) != _kernel_ERROR, "OS_File 8 makes a folder");
    T_ERRNO = 0; check (fopen (dir, "r") == 0 && T_ERRNO == EISDIR, "fopen of a folder: NULL and EISDIR");
    T_ERRNO = 0; check (fopen (dir, "w") == 0 && T_ERRNO == EISDIR, "fopen of a folder for writing: NULL and EISDIR");
    put_text (inner, "x");
    T_ERRNO = 0; check (remove (dir) == -1 && T_ERRNO == ENOTEMPTY, "remove of a folder that is not empty: -1 and ENOTEMPTY");
    check (remove (inner) == 0 && remove (dir) == 0, "remove of the file and then of the empty folder");
    remove (inner); remove (dir);
  }
  /* the OS error stays for the program */
  { _kernel_oserror *e; (void) _kernel_last_oserror (); T_ERRNO = 0; check (fopen (name2, "r") == 0 && T_ERRNO == ENOENT, "fopen of a file that is not there"); e = _kernel_last_oserror ();
    check (e != 0 && (e->errnum & 0xFF) == 0xD6, "_kernel_last_oserror () has the OS error of the failed fopen (File not found)"); }
  /* what is open at the end of a program is flushed and closed (exit), or closed (_Exit); the hook is what exit.c calls */
  f = fopen (name, "w"); if (f) fputs ("hello", f);
  if (__modlib_stdio_end_hook) __modlib_stdio_end_hook (1);
  check (__modlib_stdio_end_hook != 0 && __modlib_closeall () == 0 && file_is (name, "hello"), "the end of a program: the open file is flushed and closed");
  f = fopen (name, "w"); if (f) fputs ("lost", f);
  if (__modlib_stdio_end_hook) __modlib_stdio_end_hook (0);
  check (__modlib_closeall () == 0 && file_is (name, ""), "_Exit: the open file is closed, what was in the buffer is not written");
  /* closed streams: a bad file for every call, EOF for a second fclose, NULL */
  f = fopen (name, "r"); if (f) fclose (f);
  T_ERRNO = 0; check (fclose (0) == EOF && T_ERRNO == EBADF, "fclose (NULL): EOF and EBADF");
  /* a 2 GB file does not fit the C types: it is not opened, and what would pass 2 GB is an error */
#if !defined (T_HW)
  t_fs (1, name, 2500000000u);
  T_ERRNO = 0; check (fopen (name, "rb") == 0 && T_ERRNO == EOVERFLOW && (unsigned) length_of (name) == 2500000000u, "a file of 2.5 GB is not opened for reading: NULL and EOVERFLOW");
  T_ERRNO = 0; check (fopen (name, "r+b") == 0 && T_ERRNO == EOVERFLOW, "nor for update: EOVERFLOW");
  T_ERRNO = 0; check (fopen (name, "ab") == 0 && T_ERRNO == EOVERFLOW, "nor to append: EOVERFLOW");
  remove (name);
  t_fs (1, name, 0x7FFFFFFFu);
  f = fopen (name, "r+b");
  check (f && fseek (f, 0, SEEK_END) == 0 && ftell (f) == 0x7FFFFFFF && fgetc (f) == EOF, "a file of 2 GB - 1: the end is where the C types end");
  T_ERRNO = 0; check (f && fputc ('x', f) == 'x' && fflush (f) == EOF && ferror (f) && T_ERRNO == EFBIG, "a write that would pass 2 GB - 1: EOF, EFBIG and the error flag");
  T_ERRNO = 0; check (f && fseek (f, 0x7FFFFFFFL, SEEK_END) == -1 && T_ERRNO == EOVERFLOW, "fseek beyond 2 GB - 1: -1 and EOVERFLOW");
  if (f) fclose (f);
  remove (name);
  put_text (name, "0123456789");
  f = fopen (name, "rb");
  T_ERRNO = 0; check (f && fseek (f, 0x7FFFFFFFL, SEEK_END) == -1 && T_ERRNO == EOVERFLOW && ftell (f) == 0, "fseek (LONG_MAX, SEEK_END) on a small file: -1 and EOVERFLOW, the position is where it was");
  check (f && fgetc (f) == '0' && fseek (f, 0x7FFFFFFFL, SEEK_CUR) == -1 && fgetc (f) == '1', "fseek (LONG_MAX, SEEK_CUR): -1, the position is where it was");
  check (f && fseek (f, -1, SEEK_SET) == -1 && fseek (f, -20, SEEK_END) == -1, "a seek before the start: -1");
  if (f) fclose (f);
  /* a disc error that FileSwitch shows when it writes: fflush gives it */
  f = fopen (name, "w");
  t_fs (2, 0, 0);
  T_ERRNO = 0; check (f && fputs ("data", f) >= 0 && fflush (f) == EOF && ferror (f) && T_ERRNO == ENOSPC, "fflush: a disc error of OS_Args 255 is EOF, ENOSPC and the error flag");
  clearerr (f);
  t_fs (3, 0, 0);
  T_ERRNO = 0; check (f && fputs ("more", f) >= 0 && fflush (f) == EOF && ferror (f) && T_ERRNO == ENOSPC, "fflush: a full disc in the write is EOF, ENOSPC and the error flag");
  t_fs (4, 0, 0);
  if (f) fclose (f);
  f = fopen (name, "w");
  t_fs (3, 0, 0);
  check (f && fputs ("more", f) >= 0 && fclose (f) == EOF, "fclose: a full disc in the last write is EOF");
  t_fs (4, 0, 0);
  /* fflush (NULL) wrote everything: the catalogue has the lengths, with the files still open */
  f = fopen (name, "w"); g = fopen (name2, "w");
  if (f) fputs ("1", f);
  if (g) fputs ("22", g);
  check (fflush (0) == 0 && length_of (name) == 1 && length_of (name2) == 2, "fflush (NULL): the lengths of the open files");
  check (__modlib_closeall () == 2, "__modlib_closeall closes them");
  remove (name2);
#endif
  /* printf details: %n, a string that is not ended, widths that are too big; scanf: wide characters are not there */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-overflow"
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wformat-extra-args"
  { char o[40]; int n = -77; short sh = -1; signed char sc = -1; long ln = -1; long long ll = -1; unsigned w[4] = { 7, 7, 7, 7 };
    check (sprintf (o, "ab%n cd %d", &n, 7) == 7 && n == 2 && !t_cmp (o, "ab cd 7"), "printf %n: the count so far");
    sprintf (o, "abc%hhn%hn%ln%lln", &sc, &sh, &ln, &ll);
    check (sc == 3 && sh == 3 && ln == 3 && ll == 3, "printf %hhn %hn %ln %lln");
    { char *a = (char *) malloc (3); if (a) { a[0] = 'a'; a[1] = 'b'; a[2] = 'c'; check (snprintf (o, sizeof o, "%.3s|", a) == 4 && !t_cmp (o, "abc|"), "%.3s of an array that is not ended: nothing is read beyond the precision"); free (a); } }
    T_ERRNO = 0; check (snprintf (o, sizeof o, "%99999999999d", 1) == -1 && T_ERRNO == EOVERFLOW, "printf: a width of more than an int: -1 and EOVERFLOW");
    T_ERRNO = 0; check (snprintf (o, sizeof o, "%.99999999999d", 1) == -1 && T_ERRNO == EOVERFLOW, "printf: a precision of more than an int: -1 and EOVERFLOW");
    T_ERRNO = 0; check (snprintf (o, sizeof o, "%*d", INT_MIN, 1) == -1 && T_ERRNO == EOVERFLOW, "printf: a width of INT_MIN: -1 and EOVERFLOW");
    check (sscanf ("ab", "%ls", (wchar_t *) w) == 0 && w[0] == 7 && sscanf ("ab", "%lc", (wchar_t *) w) == 0 && w[0] == 7, "scanf %ls and %lc (wide characters) stop the scan, nothing is stored");
  }
#pragma GCC diagnostic pop
}
static void test_stdio_misc (void)
{
  const char *name = fsname (0), *name2 = fsname (1), *name3 = fsname (2);
  FILE *f, *g;
  fpos_t pos;
  char b[300];
  unsigned char big[3000], back[3000];
  sect_begin ("stdio2", 20);
  remove (name); remove (name2); remove (name3);
  T_ERRNO = 0; check (fopen (name, "r") == 0 && T_ERRNO == ENOENT, "fopen of a file that is not there: NULL and ENOENT");
  T_ERRNO = 0; check (fopen (name, "r+") == 0 && T_ERRNO == ENOENT, "fopen r+ of a file that is not there: NULL and ENOENT");
  T_ERRNO = 0; check (fopen (name, "q") == 0 && T_ERRNO == EINVAL, "a bad mode: NULL and EINVAL");
  T_ERRNO = 0; check (remove (name) == -1 && T_ERRNO == ENOENT, "remove of a file that is not there: -1 and ENOENT");
  T_ERRNO = 0; check (rename (name, name2) == -1, "rename of a file that is not there: -1");
  /* freopen: the same stream, a different file and mode */
  f = fopen (name, "w");
  check (f && fputs ("one\ntwo\n", f) >= 0, "a file of two lines");
  g = freopen (name, "r", f);
  check (g == f, "freopen gives the same stream");
  check (g && fgets (b, sizeof b, g) && !t_cmp (b, "one\n") && fgets (b, sizeof b, g) && !t_cmp (b, "two\n") && !fgets (b, sizeof b, g) && feof (g), "freopen to read: the two lines, then the end");
  if (g) fclose (g);
  /* a big buffer, odd chunks, and a long file read back with seeks */
  for (unsigned i = 0; i < sizeof big; i++) big[i] = (unsigned char) (i * 7 + (i >> 8));
  f = fopen (name, "wb");
  check (f && setvbuf (f, 0, _IOFBF, 2048) == 0, "setvbuf of a big buffer");
  { unsigned done = 0; static const unsigned chunk[] = { 1, 5, 100, 513, 31, 1000, 2 }; unsigned k = 0; while (done < sizeof big) { unsigned c = chunk[k++ % 7]; if (c > sizeof big - done) c = sizeof big - done; if (fwrite (big + done, 1, c, f) != c) break; done += c; } check (done == sizeof big, "3000 bytes written in odd chunks"); }
  check (fclose (f) == 0, "fclose of the written file");
  f = fopen (name, "rb");
  check (f && fread (back, 1, sizeof back, f) == sizeof back && !t_memcmp (back, big, sizeof big) && fread (back, 1, 1, f) == 0 && feof (f), "the file read back: the same 3000 bytes, then the end");
  check (f && fseek (f, 1000, SEEK_SET) == 0 && !feof (f) && ftell (f) == 1000 && fgetc (f) == big[1000] && ftell (f) == 1001, "fseek to 1000: not at the end, the byte there");
  check (f && fseek (f, -10, SEEK_END) == 0 && fread (back, 1, 100, f) == 10 && !t_memcmp (back, big + 2990, 10) && ftell (f) == 3000, "fseek -10 from the end: 10 bytes");
  check (f && fseek (f, 5, SEEK_CUR) == 0 && ftell (f) == 3005 && fgetc (f) == EOF, "a seek beyond the end: the position is there, and reading gives the end");
  if (f) fclose (f);
  /* the other way: an unbuffered stream, ungetc of a character that is not the one read */
  f = fopen (name, "rb");
  check (f && setvbuf (f, 0, _IONBF, 0) == 0 && fgetc (f) == big[0] && ungetc ('Z', f) == 'Z' && fgetc (f) == 'Z' && fgetc (f) == big[1], "unbuffered: ungetc of another character gives it back");
  check (f && ftell (f) == 2 && ungetc ('A', f) == 'A' && ftell (f) == 1, "ftell after ungetc is one back");
  if (f) fclose (f);
  /* update modes: write in the middle of a file, read it back */
  f = fopen (name, "r+b");
  check (f && fseek (f, 100, SEEK_SET) == 0 && fwrite ("HELLO", 1, 5, f) == 5 && fflush (f) == 0 && fseek (f, 98, SEEK_SET) == 0 && fread (back, 1, 9, f) == 9 && back[2] == 'H' && back[6] == 'O' && back[0] == big[98], "r+: write 5 bytes at 100 and read around them");
  check (f && fseek (f, 0, SEEK_END) == 0 && fwrite ("END", 1, 3, f) == 3 && ftell (f) == 3003, "r+: write at the end");
  if (f) fclose (f);
  f = fopen (name, "a+");
  check (f && ftell (f) == 0 && fgetc (f) == big[0] && fseek (f, 0, SEEK_END) == 0 && fputs ("more", f) >= 0 && fflush (f) == 0 && ftell (f) == 3007, "a+: reads from the start, writes go to the end");
  if (f) fclose (f);
  f = fopen (name, "w+");
  check (f && fputs ("abc", f) >= 0 && fseek (f, 0, SEEK_SET) == 0 && fgetc (f) == 'a' && fgetc (f) == 'b' && ftell (f) == 2, "w+: write, seek back, read");
  check (f && fgetpos (f, &pos) == 0 && fgetc (f) == 'c' && fgetc (f) == EOF && feof (f) && fsetpos (f, &pos) == 0 && !feof (f) && fgetc (f) == 'c', "fgetpos / fsetpos");
  rewind (f);
  check (ftell (f) == 0 && !ferror (f) && !feof (f), "rewind");
  if (f) fclose (f);
  /* a stream that is not there any more is a bad file: no crash for the calls that check */
  f = fopen (name, "r");
  check (f && fputc ('x', f) == EOF && ferror (f), "fputc on a stream opened for reading: EOF and the error flag");
  clearerr (f);
  check (!ferror (f) && fgetc (f) == 'a', "clearerr, then reading works");
  if (f) fclose (f);
  f = fopen (name, "w");
  check (f && fgetc (f) == EOF && ferror (f), "fgetc on a stream opened for writing: EOF and the error flag");
  if (f) fclose (f);
  /* several files at once, fflush (NULL), and closing them all */
  f = fopen (name, "w"); g = fopen (name2, "w");
  { FILE *h = fopen (name3, "w"); check (f && g && h, "three files open"); if (f) fputs ("1", f); if (g) fputs ("22", g); if (h) fputs ("333", h); }
  check (fflush (0) == 0, "fflush (NULL)");
  check (__modlib_closeall () == 3, "__modlib_closeall closes the three");
  { FILE *r = fopen (name2, "r"); check (r && fgets (b, sizeof b, r) && !t_cmp (b, "22"), "closeall wrote the buffers"); if (r) fclose (r); }
  { FILE *r = fopen (name3, "r"); check (r && fgets (b, sizeof b, r) && !t_cmp (b, "333"), "the third file has its data too"); if (r) fclose (r); }
  /* a line longer than the buffer, with fgets in small pieces */
  f = fopen (name, "w");
  { for (int i = 0; i < 700; i++) fputc ('a' + i % 26, f); fputc ('\n', f); fputs ("tail", f); fclose (f); }
  f = fopen (name, "r");
  { unsigned total = 0, pieces = 0; char *r; while ((r = fgets (b, 100, f)) != 0) { total += (unsigned) t_len (b); pieces++; } check (total == 705 && pieces == 9, "fgets in pieces of 99: 705 characters in 8 pieces and the tail"); }
  if (f) fclose (f);
  stdio_more (name, name2, name3);
  remove (name); remove (name2); remove (name3);
  sect_end ();
}

/* ======================================================================== probe: what the file system says (information, not checks).  The same lines come from the model (the host and the interpreter) and from the machine:
   the difference between them is what the model has wrong.  Every line starts with INFO. */
#include <swis.h>
static void info (const char *what, long ret)
{
  int e = T_ERRNO;
  _kernel_oserror *oe = _kernel_last_oserror ();
  char b[330];
  if (oe) snprintf (b, sizeof b, "INFO %s: ret=%ld errno=%d os=&%X \"%s\"", what, ret, e, (unsigned) oe->errnum, oe->errmess);
  else snprintf (b, sizeof b, "INFO %s: ret=%ld errno=%d", what, ret, e);
  puts (b);
}
#define PROBE(what, expr) do { long pr_; T_ERRNO = 0; (void) _kernel_last_oserror (); pr_ = (long) (expr); info (what, pr_); } while (0)
#define PROBE_V(var, what, expr) do { T_ERRNO = 0; (void) _kernel_last_oserror (); (var) = (long) (expr); info (what, (var)); } while (0)
static void infoln (const char *fmt, long a, long b)
{
  char s[300], t[330];
  snprintf (s, sizeof s, fmt, a, b);
  snprintf (t, sizeof t, "INFO %s", s);
  puts (t);
}
static void infos (const char *s)
{
  char t[330];
  snprintf (t, sizeof t, "INFO %s", s);
  puts (t);
}
static long h_status (long h)                                              /* OS_Args 254: R0 = the status word of the stream (bit 7 write, bit 6 read) */
{
  unsigned st = 0;
  _kernel_oserror *e = _swix (0x09, _INR (0, 1) | _OUT (0), 254u, (unsigned) h, &st);
  return e ? -1L : (long) st;
}
static void h_close (long h) { if (h > 0) _kernel_osfind (0, (const char *) (size_t) h); }
static void fresh (const char *name, const char *name2)                    /* NAME holds "probe-data", NAME2 is not there, nothing is locked */
{
  set_attr (name, 0x03); set_attr (name2, 0x03);
  remove (name); remove (name2);
  put_text (name, "probe-data");
  (void) _kernel_last_oserror ();
}
static void test_stdio_probe (void)
{
  const char *name = fsname (0), *name2 = fsname (1);
  static const char *const fmodes[] = { "r", "r+", "w", "a" };
  static const int attrs[] = { 0x01, 0x0B };
  static const int ops[] = { 0x4F, 0xCF, 0x8F };
  FILE *f;
  long h, h2, r;
  char b[200];
  sect_begin ("probe", 99);
  puts ("INFO ----- probe: what the file system says (information, not checks) -----");
  /* rename and remove of files in the states that matter */
  fresh (name, name2);
  PROBE ("rename of a plain file", rename (name, name2));
  PROBE ("rename of it back", rename (name2, name));
  fresh (name, name2); set_attr (name, 0x0B);
  PROBE ("rename of a locked file (attr 0x0B)", rename (name, name2));
  fresh (name, name2); set_attr (name, 0x01);
  PROBE ("rename of a read only file (attr 0x01)", rename (name, name2));
  fresh (name, name2); f = fopen (name, "w");
  PROBE ("rename of a file open for writing", rename (name, name2));
  if (f) fclose (f);
  fresh (name, name2); f = fopen (name, "r");
  PROBE ("rename of a file open for reading", rename (name, name2));
  if (f) fclose (f);
  fresh (name, name2); set_attr (name, 0x0B);
  PROBE ("remove of a locked file", remove (name));
  fresh (name, name2); set_attr (name, 0x01);
  PROBE ("remove of a read only file (attr 0x01)", remove (name));
  fresh (name, name2); f = fopen (name, "w");
  PROBE ("remove of a file open for writing", remove (name));
  if (f) fclose (f);
  fresh (name, name2); f = fopen (name, "r");
  PROBE ("remove of a file open for reading", remove (name));
  if (f) fclose (f);
  /* fopen of read only and locked files, with the library and with OS_Find itself (what the library avoids) */
  for (unsigned a = 0; a < 2; a++)
    for (unsigned m = 0; m < 4; m++)
      {
        fresh (name, name2); set_attr (name, attrs[a]);
        snprintf (b, sizeof b, "fopen \"%s\" of a file with attr 0x%02X", fmodes[m], attrs[a]);
        T_ERRNO = 0; (void) _kernel_last_oserror (); f = fopen (name, fmodes[m]); info (b, f != 0);
        if (f) fclose (f);
        infoln ("   its length afterwards: %ld (it was 10)", length_of (name), 0);
      }
  for (unsigned a = 0; a < 2; a++)
    for (unsigned o = 0; o < 3; o++)
      {
        fresh (name, name2); set_attr (name, attrs[a]);
        snprintf (b, sizeof b, "OS_Find &%X of a file with attr 0x%02X", ops[o], attrs[a]);
        PROBE_V (h, b, _kernel_osfind (ops[o], name));
        if (h > 0)
          {
            _kernel_osgbpb_block gb;
            infoln ("   OS_Args 254 status: &%lX", h_status (h), 0);
            gb.dataptr = (void *) "x"; gb.nbytes = 1; gb.fileptr = 0; gb.buf_len = 0; gb.wild_fld = 0;
            PROBE ("   OS_GBPB 2 (write 1 byte at the pointer)", _kernel_osgbpb (2, (unsigned) h, &gb));
            h_close (h);
          }
        infoln ("   its length afterwards: %ld (it was 10)", length_of (name), 0);
      }
  /* two streams on one file */
  for (unsigned a = 0; a < 3; a++)
    for (unsigned c = 0; c < 3; c++)
      {
        fresh (name, name2);
        snprintf (b, sizeof b, "OS_Find &%X, then &%X on the same file", ops[a], ops[c]);
        h = _kernel_osfind (ops[a], name);
        PROBE_V (h2, b, _kernel_osfind (ops[c], name));
        h_close (h2); h_close (h);
      }
  /* what a stream can do: the pointer beyond the end, the extent, a write through a read only stream */
  fresh (name, name2);
  h = _kernel_osfind (0xCF, name);
  if (h > 0)
    {
      infoln ("update stream: OS_Args 254 status &%lX, EXT %ld", h_status (h), _kernel_osargs (2, (unsigned) h, 0));
      PROBE ("   OS_Args 1 (PTR) to 110, 100 beyond the end", _kernel_osargs (1, (unsigned) h, 110));
      infoln ("   EXT afterwards: %ld, PTR: %ld", _kernel_osargs (2, (unsigned) h, 0), _kernel_osargs (0, (unsigned) h, 0));
      PROBE ("   OS_Args 3 (EXT) to 5", _kernel_osargs (3, (unsigned) h, 5));
      infoln ("   EXT afterwards: %ld, PTR: %ld", _kernel_osargs (2, (unsigned) h, 0), _kernel_osargs (0, (unsigned) h, 0));
      PROBE ("   OS_Args 255 (ensure)", _kernel_osargs (255, (unsigned) h, 0));
      h_close (h);
    }
  fresh (name, name2);
  h = _kernel_osfind (0x4F, name);
  if (h > 0)
    {
      _kernel_osgbpb_block gb;
      infoln ("input stream: OS_Args 254 status &%lX, EXT %ld", h_status (h), _kernel_osargs (2, (unsigned) h, 0));
      PROBE ("   OS_Args 1 (PTR) to 110, beyond the end", _kernel_osargs (1, (unsigned) h, 110));
      PROBE ("   OS_Args 1 (PTR) to 10, the end", _kernel_osargs (1, (unsigned) h, 10));
      PROBE ("   OS_Args 3 (EXT) to 5", _kernel_osargs (3, (unsigned) h, 5));
      gb.dataptr = (void *) "x"; gb.nbytes = 1; gb.fileptr = 0; gb.buf_len = 0; gb.wild_fld = 0;
      PROBE ("   OS_GBPB 2 (write)", _kernel_osgbpb (2, (unsigned) h, &gb));
      h_close (h);
    }
  fresh (name, name2);
  h = _kernel_osfind (0x8F, name);
  if (h > 0)
    {
      infoln ("output stream: OS_Args 254 status &%lX, EXT %ld", h_status (h), _kernel_osargs (2, (unsigned) h, 0));
      h_close (h);
    }
  /* the attributes, and the length of a file that is open */
  for (unsigned i = 0; i < 6; i++)
    {
      static const int set[] = { 0x03, 0x01, 0x0B, 0x00, 0x13, 0x33 };
      _kernel_osfile_block ob;
      fresh (name, name2);
      set_attr (name, set[i]);
      ob.load = ob.exec = ob.start = ob.end = 0;
      r = _kernel_osfile (17, name, &ob);
      snprintf (b, sizeof b, "OS_File 4 sets the attributes 0x%02X: OS_File 17 gives type %ld", set[i], r);
      infos (b);
      infoln ("   attributes read back: 0x%lX, length %ld", (long) (unsigned) ob.end, (long) (unsigned) ob.start);
    }
  fresh (name, name2);
  f = fopen (name, "w");
  if (f)
    {
      fputs ("0123456789", f);
      infoln ("length in the catalogue of a file open for writing, 10 bytes in the buffer: %ld (before fflush)", length_of (name), 0);
      fflush (f);
      infoln ("   after fflush: %ld", length_of (name), 0);
      fclose (f);
      infoln ("   after fclose: %ld", length_of (name), 0);
    }
  /* names: what FileSwitch makes of them */
  fresh (name, name2);
  snprintf (b, sizeof b, "%s junk", name);
  PROBE ("fopen of \"<name> junk\" (a space ends a name?)", fopen (b, "r") != 0);
  snprintf (b, sizeof b, "%s*", name);
  PROBE ("fopen of \"<name>*\" (a wild card)", fopen (b, "r") != 0);
  snprintf (b, sizeof b, "%s.nonesuch", name);
  PROBE ("fopen of \"<name>.nonesuch\" (a file is not a folder)", fopen (b, "r") != 0);
  PROBE ("fopen of \"\" (an empty name)", fopen ("", "r") != 0);
  PROBE ("fopen of \"<Wimp$ScrapDir>.MKf0\" (a variable is expanded?)", fopen ("<Wimp$ScrapDir>.MKf0", "r") != 0);
  PROBE ("fopen of \"<Nonesuch$Dir>.MKf0\" (a variable that is not set)", fopen ("<Nonesuch$Dir>.MKf0", "r") != 0);
  remove (name); remove (name2);
  puts ("INFO ----- end of the probe -----");
  sect_end ();
}
#endif

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
/* the screen and the keyboard streams (the host model and armrun.py keep what is written to the screen, and give the lines that the test pushes to OS_ReadLine) */
extern struct __FILE __modlib_stdin, __modlib_stdout, __modlib_stderr;
#if defined (T_HOSTLIB)
extern void mk_host_screen_start (void);
extern size_t mk_host_screen_take (char *buf, size_t size);
extern void mk_host_keys_push (const char *s, int len);
static void scr_start (void) { mk_host_screen_start (); }
static size_t scr_take (char *b, size_t n) { return mk_host_screen_take (b, n); }
static void key_push (const char *s, int len) { mk_host_keys_push (s, len); }
#else
#define TST_SCREEN	0x5AB04
#define TST_KEYS	0x5AB06
static void scr_start (void) { _swix (TST_SCREEN, _IN (0), 1u); }
static size_t scr_take (char *b, size_t n) { unsigned len = 0; _swix (TST_SCREEN, _INR (0, 2) | _OUT (0), 2u, b, (unsigned) n, &len); return len; }
static void key_push (const char *s, int len) { _swix (TST_KEYS, _INR (0, 1), s, (unsigned) len); }
#endif
static void test_stdio_streams (void)
{
  char b[200], s[20];
  int n = -1;
  size_t k;
  FILE *out = (FILE *) &__modlib_stdout, *err = (FILE *) &__modlib_stderr, *in = (FILE *) &__modlib_stdin, *g;
  sect_begin ("stdio3", 21);
  scr_start ();
  fputs ("ab\n", out); fputc ('x', err); fwrite ("yz", 1, 2, out); fprintf (out, "%d|%s\n", 42, "q");
  k = scr_take (b, sizeof b - 1); b[k] = 0;
  check (!t_cmp (b, "ab\n\rxyz42|q\n\r"), "the screen streams: fputs, fputc, fwrite, fprintf on stdout and stderr (a line feed is OS_NewLine: LF CR)");
#if defined (T_ARM)
  scr_start ();
  printf ("p%d", 1); puts ("s"); putchar ('c');
  k = scr_take (b, sizeof b - 1); b[k] = 0;
  check (!t_cmp (b, "p1s\n\rc"), "printf, puts and putchar write to the screen");
  scr_start ();
  n = putchar (0x10A);
  k = scr_take (b, sizeof b - 1); b[k] = 0;
  check (n == 10 && !t_cmp (b, "\n\r"), "putchar converts to unsigned char: 0x10A is a line feed, OS_NewLine, and the value is 10");
  g = freopen (fsname (0), "w", out);                                                            /* stdout becomes a file: printf, puts and putchar follow it */
  {
    int a = printf ("a%d", 1), p = fprintf (out, "b"), c = putchar ('c'), d = puts ("d"), e1, e2, e3;
    fclose (out);
    e1 = printf ("x"); e2 = putchar ('y'); e3 = puts ("z");
    if (__modlib_stdio_end_hook) __modlib_stdio_end_hook (1);                                    /* the end of a program: stdout is the screen again */
    check (g == out && a == 2 && p == 1 && c == 'c' && d == 0, "freopen (stdout): printf, fprintf, putchar and puts write to the file");
    check (e1 == -1 && e2 == EOF && e3 == EOF, "stdout closed: printf, putchar and puts fail");
    check (file_is (fsname (0), "a1bcd\n"), "the file has what the four wrote");
    remove (fsname (0));
    scr_start (); printf ("s");
    k = scr_take (b, sizeof b - 1); b[k] = 0;
    check (!t_cmp (b, "s"), "after the end of a program stdout is the screen again");
  }
#endif
  check (fgetc (out) == EOF && ferror (out), "fgetc on stdout: EOF and the error flag");
  clearerr (out);
  check (fputc ('x', in) == EOF && ferror (in), "fputc on stdin: EOF and the error flag");
  clearerr (in);
  key_push ("12 abc", 6); key_push ("second line", 11); key_push ("", 0);
  check (fscanf (in, "%d %19s", &n, s) == 2 && n == 12 && !t_cmp (s, "abc"), "fscanf (stdin): a line from the keyboard");
  check (fgets (b, sizeof b, in) && !t_cmp (b, "\n"), "fgets: the rest of the first line is its line feed");
  check (fgets (b, sizeof b, in) && !t_cmp (b, "second line\n"), "fgets: the second line");
  check (fgetc (in) == '\n', "an empty line is a line feed");
  check (fgetc (in) == EOF && feof (in), "Escape (no line left) is the end of the input");
  check (fgetc (in) == EOF, "the end of the input stays");
  clearerr (in);
  check (!feof (in), "clearerr on stdin");
  check (ftell (in) == -1 && fseek (out, 0, SEEK_SET) == -1, "ftell / fseek on the keyboard and the screen fail");
  /* a closed standard stream is a bad file; the end of a program sets them up again; the end of the input is not remembered by the next program */
  T_ERRNO = 0; check (fclose (in) == 0 && fgetc (in) == EOF && ferror (in) && T_ERRNO == EBADF, "fclose (stdin): reading it is a bad file");
  T_ERRNO = 0; check (fclose (in) == EOF && T_ERRNO == EBADF, "a second fclose of stdin: EOF and EBADF");
  if (__modlib_stdio_end_hook) __modlib_stdio_end_hook (1);
  key_push ("again", 5);
  check (fgetc (in) == 'a' && !ferror (in) && !feof (in), "after the end of a program stdin works again");
  key_push (0, -1);
  while (fgetc (in) != EOF) ;
  check (feof (in), "stdin: the end of the input");
  if (__modlib_stdio_end_hook) __modlib_stdio_end_hook (1);
  check (!feof (in), "the end of a program: the end of the input is forgotten");
  /* stderr becomes a file: the stream is the file's from now on */
  g = freopen (fsname (0), "w", err);
  check (g == err && fputs ("to a file", err) >= 0 && fclose (err) == 0, "freopen of stderr to a file");
  g = fopen (fsname (0), "r");
  check (g && fgets (b, sizeof b, g) && !t_cmp (b, "to a file"), "the file has what stderr wrote");
  if (g) fclose (g);
  remove (fsname (0));
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
#if defined (T_HW)
  { const char *d = getenv ("Wimp$ScrapDir"); size_t l = d ? t_len (d) : 0; if (l > 150) l = 150; if (d) t_mov (g_fsbase, d, l); g_fsbase[l] = '.'; t_cpy (g_fsbase + l + 1, "MK");
    if (!d) { g_fail++; puts ("FAIL the system variable Wimp$ScrapDir is not set (run this in a Task window): the stdio files go to the current directory"); t_cpy (g_fsbase, "MK"); }
    puts ("stdio: the screen streams, to be SEEN: the next two lines come from fputs (stdout) and fprintf (stderr), then a fwrite and a printf");
    fputs ("  line 1 (fputs to stdout)\n", stdout); fprintf (stderr, "  line 2 (%s to stderr, %d)\n", "fprintf", 42); { static const char l3[] = "  line 3 (fwrite)\n"; fwrite (l3, 1, sizeof l3 - 1, stdout); } printf ("  line 4 (printf %s)\n", "ok"); }
#else
  t_cpy (g_fsbase, T_FSDIR);
#endif
  test_ctype ();
  test_string ();
  test_numbers ();
  test_sort ();
  test_div ();
  test_sscanf ();
  test_printf ();
  test_stdio ();
  test_time ();
  test_limits ();
#if !defined (T_ORACLE)
  test_rand ();
  test_stdio_misc ();
  test_stdio_probe ();
#endif
#if defined (T_HOSTLIB) || (defined (T_ARM) && !defined (T_HW))
  test_swix ();
  test_clock ();
  test_stdio_streams ();
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
