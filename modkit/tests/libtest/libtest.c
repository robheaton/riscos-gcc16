/* libtest.c - tests of the C library functions of modkit (modkit/lib), as one program that is built three ways and run with the same inputs; each section prints  SECTION name n=<records> hash=<FNV of every result>
   and the three builds must print the same lines:

     T_ORACLE   the host's C library (glibc) answers: the reference.  -DEMU_LONG_BITS=32 makes it answer as a 32 bit long (ARM) would.
     T_HOSTLIB  the library's own sources compiled for the host (with ASan and UBSan) and renamed mk_*: see mk_rename.h; the test calls them under the standard names
     T_ARM      the library as ARM code (libmodkit.a), run on the interpreter (tests/libtest/armrun.py) or, in a module, on the machine
     T_HW       (with T_ARM) a runnable module for the machine: the sections that need the test SWIs of the interpreter are left out, the others are compared with the hashes of glibc's run (expected.h:
                made by pack/make-pack32.py from the oracle's output with the same SCALE), and the section hw uses the real SWIs

   The inputs come from a generator of this file (xorshift32), identical in every build; the work is scaled by SCALE.  With  -v  (host) every result is printed, to find the first difference with diff.
   The test code itself uses only its own t_* helpers for strings and memory, never the functions under test. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1                            /* (glibc: stpcpy, mempcpy, getline, timegm, fnmatch flags ... are declared) */
#endif
#ifdef T_ARM
#define __MODLIB_WANT_TIMESPEC 1                 /* (the kit's time.h has struct timespec and clock_gettime only for C11 and C++ otherwise) */
#endif
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
#include <inttypes.h>
#include <signal.h>
#include <math.h>
#include <complex.h>
#include <fenv.h>
#include <iconv.h>
#include <wchar.h>
#include <wctype.h>
#include <strings.h>
#include <fnmatch.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/timeb.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <utime.h>
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
static char g_line[9000];
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
      if (hw_expected[i].count == g_count && hw_expected[i].hash == g_hash) puts (t_cmp (g_sect, "fpmath") ? "    same as glibc's" : "    same as the host build's");        /* (fpmath has no oracle: the host build of the library is the reference) */
      else { g_fail++; puts (t_cmp (g_sect, "fpmath") ? "    DIFFERENT from glibc's (n or hash)" : "    DIFFERENT from the host build's (n or hash)"); }
    }
#endif
}
/* LT_ONLY=name,name: run only those sections (a system variable on the machine and the interpreter, an environment variable on the host); unset: all */
static int want (const char *name)
{
  const char *e = getenv ("LT_ONLY");
  size_t n = t_len (name);
  if (!e || !*e) return 1;
  while (*e)
    {
      if (t_memcmp (e, name, n) == 0 && (e[n] == ',' || e[n] == 0)) return 1;
      while (*e && *e != ',') e++;
      if (*e == ',') e++;
    }
  return 0;
}
#define RUN(name, fn) do { if (want (name)) fn (); } while (0)
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
#if defined (T_ORACLE)
  locale_t L = newlocale (LC_ALL_MASK, "C", (locale_t) 0);                 /* glibc needs a locale object; the library has the "C" locale only and takes LC_GLOBAL_LOCALE (its _l functions ignore the locale) */
#else
  locale_t L = LC_GLOBAL_LOCALE;
#endif
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
      rec_i (sgn (strcasecmp_l (a, b, L))); rec_i (sgn (strncasecmp_l (a, b, n, L)));
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


/* ======================================================================== wide characters: wchar.h, wctype.h and the multibyte functions of stdlib.h (the C locale) */
static void wgen (wchar_t *d, unsigned n, unsigned maxc)                /* a wide string of up to N - 1 characters of a small alphabet, now and then a big or a high one */
{
  unsigned len = rr (n);
  for (unsigned i = 0; i < len; i++) d[i] = rr (12) == 0 ? (wchar_t) (128 + rr (300)) : rr (30) == 0 ? (wchar_t) ((rnd () & 0x7fffffff) | 0x10000) : (wchar_t) (' ' + rr (maxc));
  d[len] = 0;
}
static void rec_w (const wchar_t *s) { size_t n = 0; while (s[n]) n++; rec_m (s, (n + 1) * sizeof (wchar_t)); }
static long woff (const wchar_t *base, const wchar_t *p) { return p ? (long) (p - base) : -1; }
static void test_wchar (void)
{
  sect_begin ("wchar", 31);
  for (unsigned i = 0; i < N (500); i++)
    {
      wchar_t a[40], b[24], set[10], buf[80], tok[40];
      case_begin (i);
      t_set (a, 0, sizeof a); t_set (b, 0, sizeof b); t_set (set, 0, sizeof set);
      wgen (a, 40, 12); wgen (b, 24, 6); wgen (set, 10, 8);
      wchar_t c = rr (6) == 0 ? 0 : rr (6) == 0 ? (wchar_t) (200 + rr (3)) : (wchar_t) (' ' + rr (12));
      unsigned n = rr (30), k = rr (40);
      rec_w (a); rec_w (b); rec_w (set); rec_i ((long) c); rec_i (n);
      rec_i ((long) wcslen (a));
      rec_i (sgn (wcscmp (a, b))); rec_i (sgn (wcsncmp (a, b, n))); rec_i (sgn (wcscoll (a, b)));
      rec_i (woff (a, wcschr (a, c))); rec_i (woff (a, wcsrchr (a, c))); rec_i (woff (a, wcsstr (a, b))); rec_i (woff (a, wmemchr (a, c, n)));
      rec_i ((long) wcsspn (a, set)); rec_i ((long) wcscspn (a, set)); rec_i (woff (a, wcspbrk (a, set)));
      t_set (buf, 0xA5, sizeof buf); wcscpy (buf, a); rec_m (buf, 40 * sizeof (wchar_t));
      t_set (buf, 0xA5, sizeof buf); wcsncpy (buf, b, n); rec_m (buf, 40 * sizeof (wchar_t));
      t_set (buf, 0xA5, sizeof buf); wcscpy (buf, a); wcscat (buf, b); rec_m (buf, 48 * sizeof (wchar_t));
      t_set (buf, 0xA5, sizeof buf); wcscpy (buf, a); wcsncat (buf, b, n); rec_m (buf, 48 * sizeof (wchar_t));
      t_set (buf, 0xA5, sizeof buf); { size_t r = wcsxfrm (buf, a, n); rec_i ((long) r); if (n > r) rec_m (buf, 40 * sizeof (wchar_t)); }
      t_set (buf, 0xA5, sizeof buf); wmemcpy (buf, a, k); wmemcpy (buf + k, a, 8); rec_m (buf, 48 * sizeof (wchar_t));
      wmemcpy (buf, a, 40); { unsigned dx = rr (8), sx = rr (8); wmemmove (buf + dx, buf + sx, k); } rec_m (buf, 48 * sizeof (wchar_t));
      wmemcpy (buf, a, 40); wmemset (buf + (k & 7), c, k); rec_m (buf, 48 * sizeof (wchar_t));
      rec_i (sgn (wmemcmp (a, b, k)));
      wcscpy (tok, a);
      { wchar_t *save = 0; int cnt = 0; wchar_t *t = wcstok (tok, set, &save); while (t && cnt < 40) { rec_w (t); cnt++; t = wcstok (0, set, &save); } rec_i (cnt); }
      /* the numbers */
      { wchar_t num[40]; char d[40];
        gen_num (d, i & 1 ? 5 : 14); for (int j = 0; j < 40; j++) num[j] = (wchar_t) (unsigned char) d[j]; if (rr (5) == 0) num[rr (8)] = (wchar_t) (200 + rr (50));
        int base = bases[rr (sizeof bases / sizeof bases[0])]; wchar_t *e = 0;
        T_ERRNO = 0; long long ll = wcstoll (num, &e, base); rec_i (ll); rec_i (e ? (long) (e - num) : -2); rec_i (norm_err ());
        T_ERRNO = 0; e = 0; unsigned long long ull = wcstoull (num, &e, base); rec_u (ull); rec_i (e ? (long) (e - num) : -2); rec_i (norm_err ());
        if (i & 1) { T_ERRNO = 0; e = 0; long l = wcstol (num, &e, base); rec_i (l); rec_i (e ? (long) (e - num) : -2); rec_i (norm_err ()); T_ERRNO = 0; e = 0; unsigned long ul = wcstoul (num, &e, base); rec_u ((unsigned) ul); rec_i (e ? (long) (e - num) : -2); rec_i (norm_err ()); } }
      { wchar_t fnum[40]; const char *fs[] = { "3.25", "  -17.5e2x", "0x1p-3", "inf", "nan", "1e400", ".5.", "", "z", "123456789012345678901234567890.5" }; const char *q = fs[rr (10)]; int j = 0; while (q[j]) { fnum[j] = (wchar_t) q[j]; j++; } fnum[j] = 0;
        wchar_t *e = 0; T_ERRNO = 0; double dv = wcstod (fnum, &e); rec_m (&dv, sizeof dv); rec_i (e ? (long) (e - fnum) : -2); rec_i (norm_err ()); }
      case_end ();
    }
  /* multibyte: every byte value, with the count of bytes given, and strings */
  for (unsigned i = 0; i < N (600); i++)
    {
      char s[24]; wchar_t w[24]; mbstate_t st; unsigned len = rr (12), n = rr (4);
      case_begin (1000 + i);
      for (unsigned j = 0; j < len; j++) s[j] = (char) (rr (9) == 0 ? rnd () : ' ' + rr (90));
      s[len] = 0;
      rec_s (s);
      for (int k = 0; k < (int) sizeof st; k++) ((char *) &st)[k] = 0;
      wchar_t wc = 77;
      T_ERRNO = 0; { int r = mblen (s, n); rec_i (r); rec_i (T_ERRNO != 0); }
      T_ERRNO = 0; { int r = mbtowc (&wc, s, n); rec_i (r); rec_i (r > 0 ? (long) wc : 0); rec_i (T_ERRNO != 0); }
      rec_i (mblen (0, 0)); rec_i (mbtowc (0, 0, 0));
      { char o[4] = { 9, 9, 9, 9 }; wchar_t x = (wchar_t) (rr (5) == 0 ? 128 + rr (200) : rr (130)); T_ERRNO = 0; int r = wctomb (o, x); rec_i (r); rec_i (T_ERRNO != 0); if (r > 0) rec_m (o, (size_t) r); rec_i (wctomb (0, x)); }
      T_ERRNO = 0; { size_t r = mbstowcs (0, s, 0); rec_i ((int) r); rec_i (T_ERRNO != 0); }
      T_ERRNO = 0; { t_set (w, 0x7f, sizeof w); size_t r = mbstowcs (w, s, n + 2); rec_i ((int) r); if (r != (size_t) -1) rec_m (w, (r < n + 2 ? r + 1 : r) * sizeof (wchar_t)); rec_i (T_ERRNO != 0); }
      T_ERRNO = 0; { size_t r = mbrtowc (&wc, s, n, &st); rec_i ((int) r); rec_i (r + 2 > 2 ? (long) wc : 0); rec_i (T_ERRNO != 0); }
      { const char *p = s; t_set (w, 0x7f, sizeof w); T_ERRNO = 0; size_t r = mbsrtowcs (w, &p, n + 1, &st); rec_i ((int) r); rec_i (p ? (long) (p - s) : -1); rec_i (T_ERRNO != 0); if (r != (size_t) -1) rec_m (w, (r < n + 1 ? r + 1 : r) * sizeof (wchar_t)); }
      { wchar_t ws[12]; for (unsigned j = 0; j < len; j++) ws[j] = (wchar_t) (rr (9) == 0 ? 128 + rr (300) : ' ' + rr (90)); ws[len] = 0; const wchar_t *p = ws; char o[16]; t_set (o, 0x7f, sizeof o); T_ERRNO = 0; size_t r = wcsrtombs (o, &p, n + 1, &st); rec_i ((int) r); rec_i (p ? (long) (p - ws) : -1); rec_i (T_ERRNO != 0); if (r != (size_t) -1) rec_m (o, r < n + 1 ? r + 1 : r);
        t_set (o, 0x7f, sizeof o); T_ERRNO = 0; r = wcstombs (o, ws, n + 1); rec_i ((int) r); rec_i (T_ERRNO != 0); if (r != (size_t) -1) rec_m (o, r < n + 1 ? r + 1 : r); }
      { int b = (int) rr (300) - 20; rec_i ((int) btowc (b)); rec_i (wctob ((wint_t) (rr (5) == 0 ? 0xFFFFFFFFu : (unsigned) rr (300)))); rec_i (mbsinit (&st)); }
      case_end ();
    }
  /* classification: every value to 300 and a few big ones */
  for (unsigned i = 0; i < N (1); i++)
    {
      static const char *names[] = { "alnum", "alpha", "blank", "cntrl", "digit", "graph", "lower", "print", "punct", "space", "upper", "xdigit", "bogus", "", "Alpha" };
      case_begin (2000 + i);
      for (unsigned c = 0; c < 330; c++)
        {
          rec_i (iswalnum (c) != 0); rec_i (iswalpha (c) != 0); rec_i (iswblank (c) != 0); rec_i (iswcntrl (c) != 0); rec_i (iswdigit (c) != 0); rec_i (iswgraph (c) != 0); rec_i (iswlower (c) != 0);
          rec_i (iswprint (c) != 0); rec_i (iswpunct (c) != 0); rec_i (iswspace (c) != 0); rec_i (iswupper (c) != 0); rec_i (iswxdigit (c) != 0); rec_i ((int) towlower (c)); rec_i ((int) towupper (c));
        }
      for (unsigned c = 0; c < 20; c++) { unsigned v = rnd (); rec_i (iswalpha (v) != 0); rec_i (iswspace (v) != 0); rec_i ((int) towlower (v)); rec_i ((int) towupper (v)); }
      for (unsigned j = 0; j < sizeof names / sizeof names[0]; j++) { wctype_t t = wctype (names[j]); rec_i (t != 0); for (unsigned c = 0; c < 130; c += 3) rec_i (iswctype (c, t) != 0); }
      { wctrans_t lo = wctrans ("tolower"), up = wctrans ("toupper"), bad = wctrans ("nothing"); rec_i (lo != 0); rec_i (up != 0); rec_i (bad != 0); for (unsigned c = 0; c < 200; c++) { rec_i ((int) towctrans (c, lo)); rec_i ((int) towctrans (c, up)); } }
      case_end ();
    }
  sect_end ();
}


/* ======================================================================== the long double functions (long double is double: each is the double function) */
#ifndef T_ORACLE
static void test_ldouble (void)
{
  sect_begin ("ldouble", 32);
  for (unsigned i = 0; i < N (300); i++)
    {
      double x = (double) rshift (8) / 1000.0 + (double) rr (50) / 7.0, y = (double) rr (40) / 9.0 + 0.5;
      long double lx = x, ly = y, lp = 0;
      double ip = 0;
      int e1 = 0, e2 = 0;
      case_begin (3000 + i);
      check (sinl (lx) == sin (x) && cosl (lx) == cos (x) && tanl (lx) == tan (x) && atanl (lx) == atan (x) && expl (lx / 100) == exp (x / 100) && sqrtl (ly) == sqrt (y), "sinl, cosl, tanl, atanl, expl, sqrtl");
      check (powl (ly, 1.5L) == pow (y, 1.5) && atan2l (lx, ly) == atan2 (x, y) && fmodl (lx, ly) == fmod (x, y) && hypotl (lx, ly) == hypot (x, y) && fabsl (lx) == fabs (x) && copysignl (ly, lx) == copysign (y, x), "powl, atan2l, fmodl, hypotl, fabsl, copysignl");
      check (floorl (lx) == floor (x) && ceill (lx) == ceil (x) && truncl (lx) == trunc (x) && roundl (lx) == round (x) && rintl (lx) == rint (x) && lroundl (lx) == lround (x) && llrintl (lx) == llrint (x), "floorl, ceill, truncl, roundl, rintl, lroundl, llrintl");
      check (frexpl (lx, &e1) == frexp (x, &e2) && e1 == e2 && ldexpl (ly, 5) == ldexp (y, 5) && scalbnl (ly, -3) == scalbn (y, -3) && modfl (lx, &lp) == modf (x, &ip) && lp == ip && ilogbl (ly) == ilogb (y), "frexpl, ldexpl, scalbnl, modfl, ilogbl");
      rec_i (sinl (lx) == sin (x)); rec_i ((long) floorl (lx));
      case_end ();
    }
  sect_end ();
}
#endif

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
#ifndef T_ORACLE
/* ---- tmpnam, tmpfile, difftime, clock_gettime, timespec_get (only the library has the checks: the names and the clocks are not glibc's) ---- */
extern const char *__modlib_tmp_prefix;
extern const char *__modlib_tmpname (FILE *f);
/* (the host's stdio.h has its own L_tmpnam, 20: the kit's is 32, and the names are as long as that) */
static int obj_type (const char *name) { _kernel_osfile_block b; b.load = b.exec = b.start = b.end = 0; return _kernel_osfile (17, name, &b); }
static void test_tmpfile (void)
{
  char pre[260], n1[32], n2[32], nm[4][32], buf[40];
  FILE *f[17];
  char *a, *b;
  int i, bad;
  struct timespec ts, ts2;
  sect_begin ("tmpfile", 50);
#ifndef T_HW
  t_cpy (pre, g_fsbase); t_cpy (pre + t_len (pre), "tmp"); if (t_len (pre) > 24) t_cpy (pre, "mktmp"); __modlib_tmp_prefix = pre;       /* (a name has at most 31 characters: too long a folder: the current one) */                 /* (the host and the interpreter have no scrap directory: the files go where the other files of the test go) */
#else
  pre[0] = 0;
#endif
  a = tmpnam (n1); b = tmpnam (0);
  check (a == n1 && b != 0 && t_len (n1) < 32 && t_len (b) < 32, "tmpnam gives names");
  if (b) t_cpy (n2, b); else n2[0] = 0;
  check (t_cmp (n1, n2) != 0, "tmpnam: two names differ");
  check (obj_type (n1) == 0 && obj_type (n2) == 0, "tmpnam: no object has the name");
  f[0] = tmpfile();
  check (f[0] != 0, "tmpfile opens a file");
  if (f[0])
    {
      const char *tn = __modlib_tmpname (f[0]);
      char name[32];
      if (tn) t_cpy (name, tn); else name[0] = 0;
      check (tn != 0 && obj_type (name) == 1, "tmpfile: the file is there while it is open");
      check (fwrite ("hello tmp", 1, 9, f[0]) == 9 && fflush (f[0]) == 0, "tmpfile: write");
      check (remove (name) == -1, "tmpfile: an open file cannot be removed");
      rewind (f[0]);
      buf[0] = 0;
      check (fgets (buf, sizeof buf, f[0]) != 0 && !t_cmp (buf, "hello tmp"), "tmpfile: the text comes back");
      check (fclose (f[0]) == 0, "tmpfile: fclose");
      check (obj_type (name) == 0, "tmpfile: the file is gone after fclose");
    }
  for (i = 0, bad = 0; i < 4; i++)                                                                     /* several at once, closed in another order */
    {
      f[i] = tmpfile ();
      if (!f[i]) { bad = 1; break; }
      t_cpy (nm[i], __modlib_tmpname (f[i]));
    }
  check (!bad && t_cmp (nm[0], nm[1]) && t_cmp (nm[1], nm[2]) && t_cmp (nm[2], nm[3]) && t_cmp (nm[0], nm[3]), "tmpfile: four at once have four names");
  for (i = 0; i < 4 && !bad; i++) check (obj_type (nm[i]) == 1, "tmpfile: each file is there");
  if (!bad)
    {
      fclose (f[2]); fclose (f[0]); fclose (f[3]); fclose (f[1]);
      for (i = 0; i < 4; i++) check (obj_type (nm[i]) == 0, "tmpfile: each file is gone after its fclose");
    }
  for (i = 0; i < 17; i++) { f[i] = tmpfile (); if (!f[i]) break; }                                    /* at most 16 at once: EMFILE */
  check (i == 16 && f[16] == 0 && T_ERRNO == EMFILE, "tmpfile: the 17th file at once is EMFILE");
  while (i-- > 0) fclose (f[i]);
  check (difftime (10, 4) == 6.0 && difftime (4, 10) == -6.0 && difftime (2000000000, -2000000000) == 4000000000.0, "difftime");
  check (clock_gettime (CLOCK_MONOTONIC, &ts) == 0 && ts.tv_nsec % 10000000 == 0 && ts.tv_nsec >= 0 && ts.tv_nsec < 1000000000, "clock_gettime (CLOCK_MONOTONIC): a centisecond count");
  check (clock_gettime (CLOCK_MONOTONIC, &ts2) == 0 && (ts2.tv_sec > ts.tv_sec || (ts2.tv_sec == ts.tv_sec && ts2.tv_nsec >= ts.tv_nsec)), "clock_gettime: the monotonic clock does not go back");
  check (clock_gettime (CLOCK_REALTIME, &ts) == 0 && ts.tv_nsec % 10000000 == 0 && time (0) - ts.tv_sec <= 1, "clock_gettime (CLOCK_REALTIME) agrees with time ()");
  check (timespec_get (&ts, TIME_UTC) == TIME_UTC && timespec_get (&ts, 0) == 0 && clock_gettime (99, &ts) == -1, "timespec_get, and a clock that is not there");
  sect_end ();
}
#endif
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

/* ======================================================================== inttypes.h: the format macros of printf and scanf for the types of <stdint.h> (the answers of glibc are the reference) */
static void test_inttypes (void)
{
  char b[200];
  sect_begin ("inttypes", 29);
  for (unsigned i = 0; i < N (300); i++)
    {
      int8_t a8 = (int8_t) rnd (); uint8_t u8 = (uint8_t) rnd (); int16_t a16 = (int16_t) rnd (); uint16_t u16 = (uint16_t) rnd ();
      int32_t a32 = (int32_t) rnd (); uint32_t u32 = rnd (); int64_t a64 = (int64_t) (((uint64_t) rnd () << 32) | rnd ()); uint64_t u64 = ((uint64_t) rnd () << 32) | rnd ();
      intmax_t am = a64 >> rr (40); uintmax_t um = u64 >> rr (40); intptr_t ap = (intptr_t) (int32_t) rnd (); uintptr_t up = (uintptr_t) (uint32_t) rnd ();                    /* (the values of a 32 bit pointer: the host's are wider) */
      int_least8_t l8 = a8; int_least16_t l16 = a16; int_least32_t l32 = a32; int_least64_t l64 = a64; int_fast8_t f8 = a8; int_fast16_t f16 = a16; int_fast32_t f32 = a32; int_fast64_t f64 = a64;
      case_begin (i);
      { intmax_t dv = (intmax_t) (rnd () | 1) * (rr (2) ? 1 : -1); imaxdiv_t q = imaxdiv (am, dv); char nb[40], *e1, *e2;                       /* the functions of <inttypes.h> (real functions of the library since 16.2.0-23) */
        rec_i (imaxabs (am / 7)); rec_i (imaxabs (-am)); rec_i (q.quot); rec_i (q.rem);
        snprintf (nb, sizeof nb, "%" PRIdMAX, am); rec_i (strtoimax (nb, &e1, 10)); rec_i (e1 - nb);
        snprintf (nb, sizeof nb, "%" PRIuMAX, um); rec_u (strtoumax (nb, &e2, 10)); rec_i (e2 - nb);
        snprintf (nb, sizeof nb, "-%" PRIxMAX "z", (uintmax_t) (am < 0 ? -am : am)); rec_i (strtoimax (nb, &e1, 16)); rec_i (e1 - nb); }
      snprintf (b, sizeof b, "%" PRId8 " %" PRIi8 " %" PRIu8 " %" PRIx8 " %" PRIX8 " %" PRIo8, a8, a8, u8, u8, u8, u8); rec_s (b);
      snprintf (b, sizeof b, "%" PRId16 " %" PRIi16 " %" PRIu16 " %" PRIx16 " %" PRIX16 " %" PRIo16, a16, a16, u16, u16, u16, u16); rec_s (b);
      snprintf (b, sizeof b, "%" PRId32 " %" PRIi32 " %" PRIu32 " %" PRIx32 " %" PRIX32 " %" PRIo32, a32, a32, u32, u32, u32, u32); rec_s (b);
      snprintf (b, sizeof b, "%" PRId64 " %" PRIi64 " %" PRIu64 " %" PRIx64 " %" PRIX64 " %" PRIo64, a64, a64, u64, u64, u64, u64); rec_s (b);
      snprintf (b, sizeof b, "%" PRIdMAX " %" PRIuMAX " %" PRIxMAX " %" PRIdPTR " %" PRIuPTR " %" PRIxPTR, am, um, um, ap, up, up); rec_s (b);
      snprintf (b, sizeof b, "%" PRIdLEAST8 " %" PRIdLEAST16 " %" PRIdLEAST32 " %" PRIdLEAST64 " %" PRIdFAST8 " %" PRIdFAST16 " %" PRIdFAST32 " %" PRIdFAST64, l8, l16, l32, l64, f8, f16, f32, f64); rec_s (b);
      { int8_t r8 = 0; uint8_t v8 = 0; int16_t r16 = 0; uint16_t v16 = 0; int32_t r32 = 0; uint32_t v32 = 0; int64_t r64 = 0; uint64_t v64 = 0; intmax_t rm = 0; uintmax_t vm = 0; intptr_t rp = 0; uintptr_t vp = 0;
        int n = 0;
        n += sscanf ("-5 200 -30000 60000 -2000000000 4000000000", "%" SCNd8 " %" SCNu8 " %" SCNd16 " %" SCNu16 " %" SCNd32 " %" SCNu32, &r8, &v8, &r16, &v16, &r32, &v32);
        rec_i (n); rec_i (r8); rec_u (v8); rec_i (r16); rec_u (v16); rec_i (r32); rec_u (v32);
        snprintf (b, sizeof b, "%" PRId64 " %" PRIu64 " %" PRIdMAX " %" PRIuMAX " %" PRIdPTR " %" PRIxPTR, a64, u64, am, um, ap, up);
        n = sscanf (b, "%" SCNd64 " %" SCNu64 " %" SCNdMAX " %" SCNuMAX " %" SCNdPTR " %" SCNxPTR, &r64, &v64, &rm, &vm, &rp, &vp);
        rec_i (n); check (r64 == a64 && v64 == u64 && rm == am && vm == um && rp == ap && vp == up, "sscanf with the SCN macros reads back what the PRI macros wrote"); }
      case_end ();
    }
  check (INT64_C (1) << 40 == 1099511627776LL && UINT64_C (18446744073709551615) == ~(uint64_t) 0, "INT64_C and UINT64_C");
  sect_end ();
}

/* ======================================================================== signal.h: signal and raise (what glibc does as well; the handler is reset to SIG_DFL before it is called by ISO C, but not by glibc: not looked at) */
static volatile int sig_hits, sig_last;
static void sig_h (int s) { sig_hits++; sig_last = s; }
static void test_signal (void)
{
  __sighandler_t old;
  sect_begin ("signal", 31);
  case_begin (0);
  sig_hits = 0; sig_last = 0;
  signal (SIGINT, SIG_DFL); signal (SIGUSR1, SIG_DFL);                                /* (a process started in the background by a shell inherits SIGINT ignored: start from the default, whoever started this) */
  old = signal (SIGINT, sig_h); rec_i (old == SIG_DFL);
  rec_i (raise (SIGINT)); rec_i (sig_hits); rec_i (sig_last == SIGINT);
  old = signal (SIGUSR1, sig_h); rec_i (old == SIG_DFL);
  old = signal (SIGUSR1, SIG_IGN); rec_i (old == sig_h);                              /* signal gives the handler that was there */
  sig_hits = 0; rec_i (raise (SIGUSR1)); rec_i (sig_hits);                            /* ignored: not called */
  old = signal (SIGUSR1, sig_h); rec_i (old == SIG_IGN);
  sig_hits = 0; rec_i (raise (SIGUSR1)); rec_i (sig_hits); rec_i (sig_last == SIGUSR1);
  rec_i (signal (0, sig_h) == SIG_ERR); rec_i (signal (999, sig_h) == SIG_ERR); rec_i (signal (-3, sig_h) == SIG_ERR);
  rec_i (raise (999) != 0);                                                           /* (raise (0) is a question for glibc: kill (getpid (), 0) says 0) */
  case_end ();
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
static _kernel_oserror *vswix_h (int swi, unsigned mask, ...) { va_list ap; _kernel_oserror *e; va_start (ap, mask); e = _vswix (swi, mask, ap); va_end (ap); return e; }
static int vswi_h (int swi, unsigned mask, ...) { va_list ap; int r; va_start (ap, mask); r = _vswi (swi, mask, ap); va_end (ap); return r; }
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
  case_begin (6);                                                                                 /* the same calls through a va_list */
  {
    unsigned a[10], b[10], fa = 0, fb = 0;
    _kernel_oserror *ea, *eb;
    for (int k = 0; k < 10; k++) a[k] = b[k] = 0x55;
    ea = _swix (TST_SWI, _INR (0, 3) | _OUTR (0, 3) | _OUT (_FLAGS), 5u, 7u, 0x100u, 3u, &a[0], &a[1], &a[2], &a[3], &fa);
    eb = vswix_h (TST_SWI, _INR (0, 3) | _OUTR (0, 3) | _OUT (_FLAGS), 5u, 7u, 0x100u, 3u, &b[0], &b[1], &b[2], &b[3], &fb);
    check (ea == 0 && eb == 0 && !t_memcmp (a, b, sizeof a) && fa == fb, "_vswix does what _swix does");
    rec_i (eb != 0); rec_m (b, sizeof b); rec_u (fb);
    check (vswi_h (TST_SWI | 0x20000, _INR (0, 1) | _RETURN (1), 3u, 4u) == _swi (TST_SWI | 0x20000, _INR (0, 1) | _RETURN (1), 3u, 4u), "_vswi does what _swi does");
    check (XOS_Bit == 0x20000u && Wimp_Initialise == 0x400C0 && XWimp_Initialise == 0x600C0 && OS_WriteC == 0 && XOS_Hardware == (0x7A | 0x20000), "swis.h has the SWI numbers");
  }
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

/* the kernel interface: blocks of the RMA, the processor mode and the interrupt functions (the machine runs a module's program in USER mode: there the mode and the I bit cannot be tested) */
static void test_kernel (void)
{
  sect_begin ("kernel", 37);
  { unsigned char *p = (unsigned char *) _kernel_RMAalloc (100), *q;
    check (p != 0, "_kernel_RMAalloc (100)");
    if (p)
      {
        for (int i = 0; i < 100; i++) p[i] = (unsigned char) (i * 7 + 1);
        q = (unsigned char *) _kernel_RMAextend (p, 300);
        check (q != 0, "_kernel_RMAextend to 300");
        if (q)
          {
            int ok = 1;
            for (int i = 0; i < 100; i++) if (q[i] != (unsigned char) (i * 7 + 1)) ok = 0;
            check (ok, "_kernel_RMAextend keeps the contents");
            for (int i = 100; i < 300; i++) q[i] = 0xA5;                          /* (all 300 bytes are there) */
            p = (unsigned char *) _kernel_RMAextend (q, 50);
            check (p != 0, "_kernel_RMAextend to 50");
            ok = p != 0;
            for (int i = 0; ok && i < 50; i++) if (p[i] != (unsigned char) (i * 7 + 1)) ok = 0;
            check (ok, "_kernel_RMAextend to less keeps the beginning");
            _kernel_RMAfree (p);
          }
        else _kernel_RMAfree (p);
      }
    q = (unsigned char *) _kernel_RMAextend (0, 64); check (q != 0, "_kernel_RMAextend of NULL is an allocation");
    if (q) check (_kernel_RMAextend (q, 0) == 0, "_kernel_RMAextend to 0 frees");
    check (_kernel_RMAalloc (0) == 0, "_kernel_RMAalloc (0) is NULL");
    _kernel_RMAfree (0); }
#if !defined (T_HW)
  check (_kernel_processor_mode () == 0x13, "the interpreter runs the library in SVC mode");
  check (_kernel_irqs_disabled () == 0, "interrupts are enabled");
  _kernel_irqs_off (); check (_kernel_irqs_disabled () != 0, "_kernel_irqs_off disables them");
  _kernel_irqs_on (); check (_kernel_irqs_disabled () == 0, "_kernel_irqs_on enables them");
#endif
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
  {                                                                                           /* long double is a double on this target: %Lf, %Le, %Lg and the L of scanf */
    char b[64]; long double ld = 1.5L, ld2 = 0; double d2 = 0; int n;
    snprintf (b, sizeof b, "%Lf|%Le|%Lg|%.2Lf", ld, ld, ld, 2.675L);
    check (!t_cmp (b, "1.500000|1.500000e+00|1.5|2.67"), "printf: %Lf %Le %Lg with a long double");
    n = sscanf ("2.5 -0.125", "%Lf %lf", &ld2, &d2);
    check (n == 2 && ld2 == 2.5L && d2 == -0.125, "sscanf: %Lf stores a long double");
    check (sizeof (long double) == sizeof (double) && strtold ("0.1", 0) == 0.1, "strtold is strtod");
  }
  sect_end ();
}
#endif

#if defined (T_HW)
/* the module that this program is on the machine (libtest-hw.cmhg): runnable, nothing to initialise */
_kernel_oserror *lt_init (const char *tail, int podule_base, void *pw) { (void) tail; (void) podule_base; (void) pw; return 0; }
#endif


/* ======================================================================== group 1 of the 4.7.4 parity list: string, time, getline and fnmatch functions (against glibc), the rest by self checks */
#if defined (T_ORACLE)
static char *ref_strichr (const char *s, int c)                              /* (glibc has no strichr / strrichr: the reference is written here, independently of the library's) */
{
  int lc = (c >= 'A' && c <= 'Z') ? c + 32 : c;
  for (;; s++)
    {
      int d = (unsigned char) *s;
      if (d >= 'A' && d <= 'Z') d += 32;
      if (d == lc) return (char *) s;
      if (!*s) return 0;
    }
}
static char *ref_strrichr (const char *s, int c)
{
  int lc = (c >= 'A' && c <= 'Z') ? c + 32 : c;
  const char *last = 0;
  for (;; s++)
    {
      int d = (unsigned char) *s;
      if (d >= 'A' && d <= 'Z') d += 32;
      if (d == lc) last = s;
      if (!*s) return (char *) last;
    }
}
# define strichr ref_strichr
# define strrichr ref_strrichr
#endif
/* ======================================================================== the C99 functions of the Shared C Library: fma, remquo, scalbln, nexttoward, nanf (and fenv, below) */
static double c99_dbl_of (uint64_t u) { union { double d; uint64_t u; } x; x.u = u; return x.d; }
static uint64_t c99_bits (double d) { union { double d; uint64_t u; } x; x.d = d; return x.u; }
static uint32_t c99_fbits (float f) { union { float f; uint32_t u; } x; x.f = f; return x.u; }
static uint64_t c99_u64 (void) { uint64_t hi = rnd (), lo = rnd (); return (hi << 32) | lo; }       /* (the order of the two calls is fixed) */
static double c99_dbl (void)
{
  /* doubles of every kind: random bits, short mantissas (ties), a narrow range of exponents (so that products and sums meet), huge and tiny exponents, subnormals */
  uint64_t m, e, sign;
  unsigned kind = rr (6), bits, sh, w;
  switch (kind)
    {
    case 0: return c99_dbl_of (c99_u64 ());
    case 1: bits = 1 + rr (31); sh = rr (22); w = rnd (); m = (uint64_t) (w & ((1u << bits) - 1)) << sh; e = 1023 - 8 + rr (17); break;
    case 2: m = c99_u64 (); e = 1023 - 30 + rr (61); break;
    case 3: m = c99_u64 (); e = rr (2) ? 1023 + 400 + rr (600) : 1023 - 500 - rr (500); break;
    case 4: m = c99_u64 (); e = rr (3); break;                                              /* subnormals and the smallest normals */
    default: w = rnd (); m = (uint64_t) (w & 0xFFFFF) << 20; e = 1023 + 510 + rr (3); break;
    }
  sign = rr (2);
  return c99_dbl_of ((sign << 63) | (e << 52) | (m & 0xFFFFFFFFFFFFFULL));
}
static void c99_quo (int qv) { int l = (qv < 0 ? -qv : qv) & 7; rec_i (l ? (qv < 0 ? -1 : 1) : 0); rec_i (l); }       /* (glibc keeps 3 bits and the sign: a quotient of 8 is 0) */
static uint64_t c99_norm (double d) { return d != d ? 0x7FF8000000000000ULL : c99_bits (d); }
static void test_c99 (void)
{
  sect_begin ("c99", 4);
  for (unsigned i = 0; i < N (6000); i++)
    {
      double x = c99_dbl (), y, z, p, r;
      int q1 = 0, q2 = 0, qv;
      y = c99_dbl (); z = c99_dbl ();
      case_begin (i);
      rec_u (c99_norm (fma (x, y, z)));
      p = x * y;
      if (p == p && p - p == 0) { rec_u (c99_norm (fma (x, y, -p))); rec_u (c99_norm (fma (x, y, -p * (1 + 1.1102230246251565e-16)))); rec_u (c99_norm (fma (x, y, -p * (1 - 2.220446049250313e-16)))); }       /* cancellation */
      rec_u (c99_norm (fma (x, 1.0, z))); rec_u (c99_norm (fma (1.0, y, z))); rec_u (c99_norm (fma (x, y, 0.0))); rec_u (c99_norm (fma (x, y, -0.0)));
      q1 = q2 = 0; r = remquo (x, y, &q1); qv = q1;
      rec_u (c99_norm (r)); c99_quo (qv);
      r = remquo (x, y / 1024.0, &q2); qv = q2;
      rec_u (c99_norm (r)); c99_quo (qv);
      { double ax = x < 0 ? -x : x; q1 = 0; r = remquo (ax, ax * 0.5, &q1); rec_u (c99_norm (r)); c99_quo (q1); q1 = 0; r = remquo (ax * 3.0, ax * 2.0, &q1); rec_u (c99_norm (r)); c99_quo (q1); }       /* ties */
      { long n1 = (long) rr (4400) - 2200, n2 = rr (2) ? 2000000000L : -2000000000L; rec_u (c99_norm (scalbln (x, n1))); rec_u (c99_norm (scalbln (x, n2))); }
      rec_u (c99_norm (nexttoward (x, y))); rec_u (c99_norm (nexttoward (x, (long double) x)));
      case_end ();
    }
  { unsigned long long b[4];
    float f0 = nanf (""), f1 = nanf ("123"), f2 = nanf ("0x7fffff"), f3 = nanf ("junk");
    b[0] = c99_fbits (f0); b[1] = c99_fbits (f1); b[2] = c99_fbits (f2); b[3] = c99_fbits (f3);
    case_begin (0);
    rec_u (b[0]); rec_u (b[1]); rec_u (b[2]); rec_u (b[3]);
    case_end (); }
  sect_end ();
}

/* ======================================================================== erf, erfc, lgamma, tgamma against the values of Python's math module (tolerances: the ones the library claims) */
#include "libtest-gamma.inc"
static double gm_err (double got, double want, double tiny)       /* the relative error, with an absolute floor of TINY for results near zero */
{
  double d = got - want, s = (want < 0 ? -want : want);
  if (got != got || want != want) return (got != got) == (want != want) ? 0.0 : 1e9;
  if (got == want) return 0.0;
  if (d - d != 0) return 1e9;
  return (d < 0 ? -d : d) / (s > tiny ? s : tiny);
}
static void test_gamma (void)
{
  double w_erf = 0, w_erfc = 0, w_lg = 0, w_tg = 0, w_tgs = 0;
  int bad = 0;
  sect_begin ("gamma", 5);
  for (unsigned i = 0; i < 400; i++)
    {
      const struct gm_case *c = &gm_cases[i];
      double e, ec, lg, tg, a;
      int sg = 0, ok;
      case_begin (i);
      e = erf (c->x); ec = erfc (c->x); lg = lgamma_r (c->x, &sg); tg = tgamma (c->x);
      a = gm_err (e, c->erf, 1e-300); if (a > w_erf) w_erf = a; ok = a < 4e-16 * 3; rec_i (ok); bad += !ok;
      a = gm_err (ec, c->erfc, 1e-300); if (a > w_erfc) w_erfc = a; ok = a < 4e-16 * 3; rec_i (ok); bad += !ok;
      if (c->lgamma == c->lgamma) { a = gm_err (lg, c->lgamma, 1e-2); if (a > w_lg) w_lg = a; ok = a < 1e-13; } else ok = 1;       /* (near the zeros of lgamma, at 1 and 2, the error is counted against 1e-2) */
      rec_i (ok); bad += !ok;
      if (c->tgamma == c->tgamma) { a = gm_err (tg, c->tgamma, 1e-300); if (a > w_tg) w_tg = a; if (c->x > 0 && a > w_tgs) w_tgs = a; ok = a < (c->x > 20 || c->x < -20 ? 2e-13 : 1e-14); if (g_verbose && !ok) printf ("  gamma  tgamma (%a = %.17g) is %.17g, wanted %.17g\n", c->x, c->x, tg, c->tgamma); } else ok = 1;
      rec_i (ok); bad += !ok;
      rec_i (c->x < 0 && lg == lg && ((tg < 0) != (sg < 0)) && c->tgamma == c->tgamma && tg != 0 ? 0 : 1);       /* the sign of lgamma_r is the sign of tgamma */
      case_end ();
    }
  if (g_verbose) printf ("  gamma  worst relative error: erf %.3g erfc %.3g lgamma %.3g tgamma %.3g (positive x: %.3g)\n", w_erf, w_erfc, w_lg, w_tg, w_tgs);
  check (!bad, "erf, erfc, lgamma and tgamma against Python's math module");
  check (erf (0.0) == 0.0 && erf (INFINITY) == 1.0 && erf (-INFINITY) == -1.0 && erfc (INFINITY) == 0.0 && erfc (-INFINITY) == 2.0 && erfc (0.0) == 1.0 && isnan (erf (NAN)), "erf and erfc: the special values");
  check (tgamma (1.0) == 1.0 && tgamma (2.0) == 1.0 && tgamma (5.0) == 24.0 && tgamma (23.0) == 1124000727777607680000.0 && tgamma (0.5) > 1.77245385090551 && tgamma (0.5) < 1.77245385090552 && isinf (tgamma (172.0)) && isinf (tgamma (0.0)) && isnan (tgamma (-2.0)) && isnan (tgamma (-INFINITY)) && tgamma (INFINITY) == INFINITY, "tgamma: integers, 1/2 and the special values");
  { int s = 0; check (lgamma_r (1.0, &s) == 0.0 && s == 1 && lgamma_r (2.0, &s) == 0.0 && isinf (lgamma_r (0.0, &s)) && isinf (lgamma_r (-3.0, &s)) && lgamma_r (-0.5, &s) > 1.26 && s == -1 && lgamma_r (-1.5, &s) > 0.8 && s == 1, "lgamma_r: zeros, poles and signs"); }
  sect_end ();
}

static void test_g1 (void)
{
  static const char alpha[] = "abcABC xyz.-";
  sect_begin ("g1", 17);
  for (unsigned i = 0; i < N (500); i++)
    {
      char a[48], b[48], d[64];
      unsigned la = rr (30), n = rr (40), j, k;
      case_begin (i);
      for (j = 0; j < la; j++) a[j] = alpha[rr (12)];
      a[la] = 0;
      t_set (d, 0xA5, sizeof d); { char *e = stpcpy (d, a); rec_i (e - d); rec_m (d, 48); }
      t_set (d, 0xA5, sizeof d); { char *e = stpncpy (d, a, n); rec_i (e - d); rec_m (d, 48); }
      k = rr (45);
      t_set (d, 0xA5, sizeof d); { char *e = mempcpy (d, a, k < la + 1 ? k : la + 1); rec_i (e - d); rec_m (d, 48); }
      t_mov (b, a, la + 1); if (la && rr (2)) b[rr (la)] ^= 0x20;
      k = rr (la + 2); { int r = bcmp (a, b, k); rec_i (r != 0); }
      { unsigned r1 = rnd (), r2 = rnd (), r3 = rnd (), r4 = rr (32), r5 = rr (64);             /* (the draws in a fixed order: the order of evaluation of the operands is not) */
	int v = (int) r1 >> r4; long lv = (long) v; long long ll = (long long) (((unsigned long long) r2 << 32) | r3) >> r5;
	rec_i (ffs (v)); rec_i (ffsl (lv)); rec_i (ffsll (ll)); if (rr (8) == 0) { rec_i (ffs (0)); rec_i (ffsll (0)); } }
      { int c = rr (8) == 0 ? 0 : alpha[rr (12)]; if (rr (3) == 0 && c >= 'a' && c <= 'z') c -= 32;
	{ char *r = strichr (a, c); rec_i (r ? r - a : -1); r = strrichr (a, c); rec_i (r ? r - a : -1); } }
      case_end ();
    }
  /* the reentrant time functions, timegm and dysize */
  for (unsigned i = 0; i < N (400); i++)
    {
      struct tm tm, tm2, tm3;
      char ab[64];
      long long t = (long long) (rnd () % 4122167296u) - 2061083648LL;
      time_t tt = (time_t) t;
      case_begin (1000 + i);
      rec_i (t);
      gmtime_r (&tt, &tm); rec_tm (&tm);
      localtime_r (&tt, &tm2); rec_tm (&tm2);
      t_set (ab, 0x5A, sizeof ab); { char *r = asctime_r (&tm, ab); rec_i (r == ab); rec_s (ab); }
      t_set (ab, 0x5A, sizeof ab); { char *r = ctime_r (&tt, ab); rec_i (r == ab); rec_s (ab); }
      copy_tm (&tm3, &tm);
      if (rr (5) == 0) tm3.tm_year += 8100 + (int) rr (2000);                       /* a year of five digits does not fit the 26 bytes */
      T_ERRNO = 0; t_set (ab, 0x5A, sizeof ab);
      { char *r = asctime_r (&tm3, ab); rec_i (r != 0); if (r) rec_s (ab); else rec_i (T_ERRNO == EOVERFLOW); }
      copy_tm (&tm2, &tm);
      tm2.tm_mday += (int) rr (70) - 35; tm2.tm_hour += (int) rr (50) - 25; tm2.tm_min += (int) rr (130) - 65; tm2.tm_sec += (int) rr (200) - 100;
      tm2.tm_mon += (int) rr (30) - 15; tm2.tm_year += (int) rr (3) - 1;
      { time_t r = timegm (&tm2); rec_i ((long long) r); rec_tm (&tm2); }
      { int y = 1 + (int) rr (4000); rec_i (y); rec_i (dysize (y)); }
      case_end ();
    }
  sect_end ();
}

/* fnmatch against glibc: patterns made of tokens (so that a bracket expression is always one that both sides read the same way) */
static void test_fnmatch (void)
{
  static const char *const tok[] = { "a", "b", "c", "A", "B", ".", "/", "*", "?", "[abc]", "[!a-c]", "[^b]", "[a-c]", "[[:alpha:]]", "[[:upper:]x]", "\\*", "\\a", "[]a]", "[", "[a", "*", "?", "a", "b", "[A-B]", "[!.]", "[[:digit:].]" };
  static const char nalpha[] = "abcAB./*[\\";
  sect_begin ("fnmatch", 19);
  for (unsigned i = 0; i < N (6000); i++)
    {
      char p[64], s[24];
      unsigned np = 1 + rr (6), ns = rr (8), j, flags = rr (32);
      size_t l = 0;
      case_begin (i);
      for (j = 0; j < np; j++) { const char *t = tok[rr (sizeof tok / sizeof tok[0])]; while (*t) p[l++] = *t++; }
      p[l] = 0;
      for (j = 0; j < ns; j++) s[j] = nalpha[rr (rr (6) ? 7 : 10)];
      s[ns] = 0;
      rec_s (p); rec_s (s); rec_i (flags);
      rec_i (fnmatch (p, s, (int) flags));
      case_end ();
    }
  sect_end ();
}

/* getline and getdelim against glibc, on files of random bytes (with newlines, colons and NUL bytes) */
static void test_getline (void)
{
  sect_begin ("getline", 23);
  for (unsigned i = 0; i < N (150); i++)
    {
      const char *name = fsname (3);
      unsigned n = rr (300);
      char *line = 0;
      size_t cap = 0;
      FILE *f;
      int delim = rr (3) == 0 ? ':' : '\n';
      case_begin (i);
      put_file (name, n);
      f = fopen (name, "rb");
      rec_i (f != 0);
      if (f)
	{
	  for (unsigned k = 0; k < 400; k++)
	    {
	      int r = (int) (delim == '\n' ? getline (&line, &cap, f) : getdelim (&line, &cap, delim, f));      /* (the kit's ssize_t is int; the host's is long: the upper half of the result is not the library's) */
	      rec_i (r);
	      if (r < 0) break;
	      rec_m (line, (size_t) r + 1);
	    }
	  rec_i (feof (f) != 0);
	  fclose (f);
	}
      free (line);
      remove (name);
      case_end ();
    }
  sect_end ();
}


#if defined (T_ARM)
/* group 1, the functions that need the OS or that glibc spells differently: checked against answers worked out here */
static int streq (const char *a, const char *b) { return a && b && !t_cmp (a, b); }
static int g1_order[4], g1_n;
static void g1_q1 (void) { g1_order[g1_n++] = 1; }
static void g1_q2 (void) { g1_order[g1_n++] = 2; }
static void test_g1self (void)
{
  sect_begin ("g1self", 1);
  { fenv_t env, env2; fexcept_t fl;
    feclearexcept (FE_ALL_EXCEPT);
    check (fetestexcept (FE_ALL_EXCEPT) == 0 && fegetround () == FE_TONEAREST && fesetround (FE_TONEAREST) == 0 && fesetround (FE_UPWARD) != 0 && fegetround () == FE_TONEAREST, "fenv: no flags at the start, the rounding mode is FE_TONEAREST and stays");
    check (feraiseexcept (FE_INEXACT | FE_OVERFLOW) == 0 && fetestexcept (FE_ALL_EXCEPT) == (FE_INEXACT | FE_OVERFLOW) && fetestexcept (FE_OVERFLOW) == FE_OVERFLOW && fetestexcept (FE_INVALID) == 0, "feraiseexcept and fetestexcept");
    check (fegetexceptflag (&fl, FE_OVERFLOW | FE_INVALID) == 0 && fl == FE_OVERFLOW && feclearexcept (FE_OVERFLOW) == 0 && fetestexcept (FE_ALL_EXCEPT) == FE_INEXACT && fesetexceptflag (&fl, FE_OVERFLOW) == 0 && fetestexcept (FE_ALL_EXCEPT) == (FE_INEXACT | FE_OVERFLOW), "fegetexceptflag, feclearexcept and fesetexceptflag");
    check (fegetenv (&env) == 0 && feclearexcept (FE_ALL_EXCEPT) == 0 && fetestexcept (FE_ALL_EXCEPT) == 0 && fesetenv (&env) == 0 && fetestexcept (FE_ALL_EXCEPT) == (FE_INEXACT | FE_OVERFLOW), "fegetenv and fesetenv");
    check (feholdexcept (&env2) == 0 && fetestexcept (FE_ALL_EXCEPT) == 0 && feraiseexcept (FE_UNDERFLOW) == 0 && feupdateenv (&env2) == 0 && fetestexcept (FE_ALL_EXCEPT) == (FE_INEXACT | FE_OVERFLOW | FE_UNDERFLOW), "feholdexcept and feupdateenv (the flags raised in between are kept)");
    check (fesetenv (FE_DFL_ENV) == 0 && fetestexcept (FE_ALL_EXCEPT) == 0, "fesetenv (FE_DFL_ENV)"); }
  { struct timeval tv = { 0, 0 }; struct timezone tz = { 0, 0 }; struct timespec ts = { 0, 0 };
    errno = 0; check (sethostname ("host", 4) == -1 && errno == EPERM, "sethostname: -1 and EPERM (as in UnixLib: a module does not set the name of the machine)");
    errno = 0; check (setdomainname ("dom", 3) == -1 && errno == EPERM, "setdomainname: -1 and EPERM");
    errno = 0; check (settimeofday (&tv, &tz) == -1 && errno == EPERM, "settimeofday: -1 and EPERM");
    errno = 0; check (settimeofday (0, 0) == -1 && errno == EPERM, "settimeofday (NULL, NULL): -1 and EPERM");
    errno = 0; check (clock_settime (CLOCK_REALTIME, &ts) == -1 && errno == EPERM, "clock_settime (CLOCK_REALTIME): -1 and EPERM");
    errno = 0; check (clock_settime (CLOCK_MONOTONIC, &ts) == -1 && errno == EPERM, "clock_settime (CLOCK_MONOTONIC): -1 and EPERM");
    errno = 0; check (clock_settime (77, &ts) == -1 && errno == EINVAL, "clock_settime of a clock that does not exist: -1 and EINVAL"); }
  check (strcasecmp_l ("Hello", "hELLO", LC_GLOBAL_LOCALE) == 0 && strcasecmp_l ("a", "b", LC_GLOBAL_LOCALE) < 0 && strcasecmp_l ("b", "A", LC_GLOBAL_LOCALE) > 0
	 && strncasecmp_l ("ABCx", "abcy", 3, LC_GLOBAL_LOCALE) == 0 && strncasecmp_l ("ABCx", "abcy", 4, LC_GLOBAL_LOCALE) < 0 && strncasecmp_l ("xyz", "XYZ", 0, LC_GLOBAL_LOCALE) == 0,
	 "strcasecmp_l and strncasecmp_l: ASCII case folding, the locale is ignored");
  check (streq (strerror_l (ENOENT, LC_GLOBAL_LOCALE), strerror (ENOENT)) && streq (strerror_l (EPROTO, LC_GLOBAL_LOCALE), "Protocol error") && streq (strerror_l (999, LC_GLOBAL_LOCALE), "Unknown error 999"),
	 "strerror_l: the texts of strerror");
  check (streq (strsignal (SIGINT), "Interrupt") && streq (strsignal (SIGSEGV), "Segmentation fault") && streq (strsignal (SIGTERM), "Terminated") && streq (strsignal (99), "Unknown signal 99")
	 && streq (strsignal (0), "Unknown signal 0") && streq (strsignal (-5), "Unknown signal -5"), "strsignal: the texts and an unknown number");
  check (sys_nsig == NSIG && streq (sys_siglist[SIGABRT], "Aborted") && streq (sys_siglist[SIGUSR2], "User defined signal 2"), "sys_siglist and sys_nsig");
  { int bad = 0; for (int e = 0; e < sys_nerr; e++) if (!sys_errlist[e] || !streq (strerror (e), sys_errlist[e])) bad++; check (sys_nerr == 99 && !bad, "sys_errlist: every entry is strerror's text"); }
  check (streq (strerror (EPROTO), "Protocol error") && streq (strerror (ENODATA), "No data available") && streq (strerror (ENOLINK), "Link has been severed") && streq (strerror (EMULTIHOP), "Multihop attempted")
	 && streq (strerror (ECANCELED), "Operation canceled") && streq (strerror (ENOTRECOVERABLE), "State not recoverable") && streq (strerror (EOWNERDEAD), "Owner died"), "strerror: the seven POSIX codes that UnixLib lacks");
  check (streq (strerror (ENOENT), "No such file or directory") && streq (strerror (ETIMEDOUT), "Connection timed out") && streq (strerror (EILSEQ), "Invalid or incomplete multibyte or wide character")
	 && streq (strerror (100), "Unknown error 100") && streq (strerror (-1), "Unknown error -1") && streq (strerror (0), "No error"), "strerror: the texts");
  { char b[64]; int r;
    r = strerror_r (ENOENT, b, sizeof b); check (r == 0 && streq (b, "No such file or directory"), "strerror_r: the text");
    r = strerror_r (ENOENT, b, 4); check (r == ERANGE && streq (b, "No "), "strerror_r: a short buffer gives ERANGE and the text cut");
    r = strerror_r (999, b, sizeof b); check (r == EINVAL && streq (b, "Unknown error 999"), "strerror_r: an unknown number is EINVAL");
    r = strerror_r (EPERM, b, 0); check (r == ERANGE, "strerror_r: no room at all is ERANGE"); }
  { const char *v;
    check (setenv ("G1Test$A", "one", 1) == 0 && (v = getenv ("G1Test$A")) != 0 && streq (v, "one"), "setenv sets a system variable");
    check (setenv ("G1Test$A", "two", 0) == 0 && streq (getenv ("G1Test$A"), "one"), "setenv with replace 0 keeps the value");
    check (setenv ("G1Test$A", "two", 1) == 0 && streq (getenv ("G1Test$A"), "two"), "setenv with replace 1 replaces it");
    check (putenv ("G1Test$B=three") == 0 && streq (getenv ("G1Test$B"), "three"), "putenv NAME=value sets it");
    check (putenv ("G1Test$B") == 0 && getenv ("G1Test$B") == 0, "putenv with no = removes it");
    check (unsetenv ("G1Test$A") == 0 && getenv ("G1Test$A") == 0, "unsetenv removes it");
    check (unsetenv ("G1Test$A") == 0, "unsetenv of a variable that is not there is 0");
    T_ERRNO = 0; check (setenv ("", "x", 1) == -1 && T_ERRNO == EINVAL, "setenv: an empty name is EINVAL");
    T_ERRNO = 0; check (setenv ("a=b", "x", 1) == -1 && T_ERRNO == EINVAL, "setenv: a name with = is EINVAL");
    T_ERRNO = 0; check (unsetenv ("a=b") == -1 && T_ERRNO == EINVAL, "unsetenv: a name with = is EINVAL"); }
#if !defined (T_HW)
  { char b[64];                                                               /* (not on the machine: Inet$HostName and Inet$LocalDomain are the network's) */
    _kernel_setenv ("Inet$HostName", "g1host"); _kernel_setenv ("Inet$LocalDomain", 0);
    check (gethostname (b, sizeof b) == 0 && streq (b, "g1host"), "gethostname: Inet$HostName");
    _kernel_setenv ("Inet$LocalDomain", ".example.org");
    check (gethostname (b, sizeof b) == 0 && streq (b, "g1host.example.org"), "gethostname: the domain is added (a leading dot is dropped)");
    check (getdomainname (b, sizeof b) == 0 && streq (b, ".example.org"), "getdomainname: Inet$LocalDomain");
    _kernel_setenv ("Inet$HostName", "a.b.c");
    check (gethostname (b, sizeof b) == 0 && streq (b, "a.b.c"), "gethostname: a name with a dot is as it is");
    _kernel_setenv ("Inet$LocalDomain", 0);
    check (getdomainname (b, sizeof b) == 0 && streq (b, "b.c"), "getdomainname: what follows the first dot of the host name");
    _kernel_setenv ("Inet$HostName", "nodot");
    T_ERRNO = 0; check (getdomainname (b, sizeof b) == -1 && T_ERRNO == ENOSYS, "getdomainname: none is ENOSYS");
    t_set (b, 0x55, sizeof b); T_ERRNO = 0; check (gethostname (b, 3) == -1 && T_ERRNO == ENAMETOOLONG && b[0] == 'n' && b[1] == 'o' && b[2] == 'd' && b[3] == 0x55, "gethostname: too short copies what fits, without a NUL");
    _kernel_setenv ("Inet$HostName", 0); }
#else
  { char b[64]; const char *h = getenv ("Inet$HostName");
    check (gethostname (b, sizeof b) == 0 && b[0] != 0 && (!h || !strncmp (b, h, t_len (h) < sizeof b ? t_len (h) : 0)), "gethostname: the machine's name starts with Inet$HostName"); }
#endif
  { struct timeval tv; struct timezone tz; struct timeb tb; struct tms tms; struct timespec ts; clock_t c; time_t t1;
    t1 = time (0);
    check (gettimeofday (&tv, &tz) == 0 && tv.tv_usec >= 0 && tv.tv_usec < 1000000 && tv.tv_usec % 10000 == 0 && tz.tz_minuteswest == 0 && tz.tz_dsttime == 0, "gettimeofday: the clock in 10 ms steps, no time zone");
    check (tv.tv_sec - t1 >= 0 && tv.tv_sec - t1 <= 2, "gettimeofday agrees with time ()");
    check (ftime (&tb) == 0 && tb.millitm % 10 == 0 && tb.millitm < 1000 && tb.timezone == 0 && tb.dstflag == 0 && tb.time - t1 >= 0 && tb.time - t1 <= 2, "ftime");
    c = times (&tms);
    check (tms.tms_utime == c && tms.tms_stime == 0 && tms.tms_cutime == 0 && tms.tms_cstime == 0, "times: the processor time is clock ()");
    check (clock_getres (CLOCK_REALTIME, &ts) == 0 && ts.tv_sec == 0 && ts.tv_nsec == 10000000, "clock_getres: 10 ms");
    T_ERRNO = 0; check (clock_getres (99, &ts) == -1 && T_ERRNO == EINVAL, "clock_getres: an unknown clock is EINVAL");
    timezone = 5; daylight = 5; tzset ();
    check (timezone == 0 && daylight == 0 && streq (tzname[0], "GMT") && streq (tzname[1], "GMT"), "tzset: no time zone"); }
  { static const unsigned al[] = { 1, 2, 4, 8, 16, 32, 64, 256, 4096, 65536 }; int bad = 0;
    for (unsigned k = 0; k < sizeof al / sizeof al[0]; k++)
      for (unsigned z = 0; z < 3; z++)
	{
	  size_t n = z == 0 ? 1 : z == 1 ? 100 : 5000, m;
	  unsigned mask = (al[k] > 8 ? al[k] : 8) - 1;
	  char *p = aligned_alloc (al[k], n), *q;
	  if (!p || ((unsigned) p & mask)) { bad++; continue; }
	  t_set (p, 0x5A, n);
	  q = realloc (p, n * 2);                                                  /* (a block of aligned_alloc is a block of malloc: realloc and free work) */
	  if (!q) { bad++; continue; }
	  for (m = 0; m < n; m++) if (q[m] != 0x5A) { bad++; break; }
	  free (q);
	}
    check (!bad, "aligned_alloc: aligned blocks that realloc and free take");
    T_ERRNO = 0; check (aligned_alloc (3, 16) == 0 && T_ERRNO == EINVAL, "aligned_alloc: an alignment that is not a power of two is EINVAL");
    check (aligned_alloc (0, 16) == 0, "aligned_alloc: alignment 0 fails"); }
  check (at_quick_exit (g1_q1) == 0 && at_quick_exit (g1_q2) == 0 && g1_n == 0, "at_quick_exit registers functions (quick_exit itself ends the program)");
  check (fileno (stdin) == 0 && fileno (stdout) == 1 && fileno (stderr) == 2, "fileno of the standard streams");
  { const char *name = fsname (3);
    FILE *f = fopen (name, "wb");
    T_ERRNO = 0;
    check (f != 0 && fileno (f) >= 3, "fileno of a file is 3 or more");
    if (f) { fputs ("0123456789", f); fclose (f); }
    f = fopen (name, "rb");
    check (f != 0 && fseeko (f, 4, SEEK_SET) == 0 && ftello (f) == 4 && fgetc (f) == '4' && ftello (f) == 5, "fseeko and ftello");
    if (f) fclose (f);
    remove (name); }
  T_ERRNO = 0; check (fileno (0) == -1 && T_ERRNO == EBADF, "fileno (NULL) is EBADF");
  check (getprogname () != 0, "getprogname gives a string");
#if defined (T_HW)
  { char b[300]; char *r = realpath ("$", b); const char *e = realpath ("NoSuchObjectG1", b) ? "" : "x";
    check (r == b && b[0] != 0 && strchr (b, '$') != 0, "realpath of $ is the canonical name of the root");
    T_ERRNO = 0; check (realpath ("NoSuchObjectG1", 0) == 0 && T_ERRNO == ENOENT && e[0], "realpath of an object that does not exist is ENOENT");
    r = realpath ("$", 0); check (r != 0 && b[0] != 0 && streq (r, b), "realpath with no buffer gives a malloc block"); free (r); }
#endif
  sect_end ();
}
#endif


/* ======================================================================== group 2: the descriptor functions (open, read, write, lseek, ftruncate, close, unlink ...) */
#if defined (T_HOSTLIB)                                                   /* (the library has the UnixLib numbers of the flags; the host's headers are glibc's) */
# define T_O_CREAT 0x0200
# define T_O_EXCL 0x0800
# define T_O_TRUNC 0x0400
# define T_O_APPEND 0x0008
#else
# define T_O_CREAT O_CREAT
# define T_O_EXCL O_EXCL
# define T_O_TRUNC O_TRUNC
# define T_O_APPEND O_APPEND
#endif
static int norm_err2 (void)                                               /* the errors that the two C libraries both have, by a number of their own */
{
  switch (T_ERRNO)
    {
    case 0: return 0;
    case EINVAL: return 2;
    case EBADF: return 4;
    case ENOENT: return 5;
    case EEXIST: return 6;
    case EISDIR: return 7;
    case ESPIPE: return 8;
    case EACCES: return 9;
    case ENOTTY: return 10;
    default: return 3;
    }
}
/* random sequences of the descriptor calls on a file, the same against glibc and the library: open with every combination of flags, write, read, lseek, ftruncate, close, unlink; the contents of the file at the end */
static void test_fd (void)
{
  static const int flagsets[] = { 0, 1, 2, 1 | T_O_CREAT | T_O_TRUNC, 2 | T_O_CREAT | T_O_TRUNC, 1 | T_O_CREAT | T_O_APPEND, 2 | T_O_CREAT | T_O_APPEND, 2 | T_O_CREAT, 1 | T_O_CREAT | T_O_EXCL, 2 | T_O_TRUNC,
				  1 | T_O_APPEND, 0 | T_O_CREAT };
  const char *name = fsname (0);
  sect_begin ("fd", 29);
  for (unsigned i = 0; i < N (800); i++)
    {
      unsigned fi = rr (12);
      int fl = flagsets[fi], fd;
      unsigned nops;
      case_begin (i);
      remove (name);
      if (rr (4) != 0) put_file (name, rr (3) == 0 ? rr (30) : rr (1300));
      rec_i (fi);                                                        /* (the flags themselves have other numbers in the two libraries) */
      fd = open (name, fl, 0666);
      rec_i (fd >= 3);
      if (fd < 0) { rec_i (norm_err2 ()); rec_i (unlink (name) != 0); case_end (); continue; }
      nops = 1 + rr (12);
      for (unsigned k = 0; k < nops; k++)
	{
	  unsigned op = rr (9);
	  rec_i (-1000 - (long long) op);
	  switch (op)
	    {
	    case 0: case 7:                                                    /* write */
	      {
		unsigned char b[700];
		unsigned n = op == 7 ? 1 : rr (3) == 0 ? rr (300) : rr (40);
		int r;
		rtext (b, n);
		T_ERRNO = 0;
		r = (int) write (fd, b, n);
		rec_i (r);
		if (r < 0) rec_i (norm_err2 ());
		break;
	      }
	    case 1: case 6:                                                    /* read */
	      {
		unsigned char b[700];
		unsigned n = op == 6 ? 1 : rr (4) == 0 ? rr (400) : rr (50);
		int r;
		t_set (b, 0xA5, sizeof b);
		T_ERRNO = 0;
		r = (int) read (fd, b, n);
		rec_i (r);
		if (r > 0) rec_m (b, (size_t) r); else if (r < 0) rec_i (norm_err2 ());
		break;
	      }
	    case 2:                                                            /* lseek */
	      {
		int whence = (int) rr (4) == 3 ? 7 : (int) rr (3);          /* (a whence that is none of the three: EINVAL) */
		long off = (long) rr (300) - (whence == SEEK_SET ? 20 : 150);
		long long r;
		T_ERRNO = 0;
		r = (long long) (long) lseek (fd, (off_t) off, whence);
		rec_i (r);
		if (r < 0) rec_i (norm_err2 ());
		break;
	      }
	    case 3:                                                            /* ftruncate */
	      {
		long len = rr (3) == 0 ? (long) rr (2000) : (long) rr (300);
		int r;
		if (rr (20) == 0) len = -1;
		T_ERRNO = 0;
		r = ftruncate (fd, (off_t) len);
		rec_i (r);
		if (r < 0) rec_i (norm_err2 ());
		break;
	      }
	    case 4: rec_i ((long long) (long) lseek (fd, 0, SEEK_CUR)); break;
	    case 5: rec_i ((long long) (long) lseek (fd, 0, SEEK_END)); break;
	    default:
	      {
		int r;
		T_ERRNO = 0;
		r = (int) write (fd, "", 0);
		rec_i (r);
		if (r < 0) rec_i (norm_err2 ());
		break;
	      }
	    }
	}
      rec_i (close (fd));
      T_ERRNO = 0; rec_i (close (fd)); rec_i (norm_err2 ());              /* the second close: EBADF */
      T_ERRNO = 0; rec_i ((int) read (fd, name, 1)); rec_i (norm_err2 ());
      rec_file (name);
      rec_i (unlink (name));
      T_ERRNO = 0; rec_i (unlink (name)); rec_i (norm_err2 ());
      case_end ();
    }
  sect_end ();
}


#if defined (T_ARM)
/* group 2, the descriptor functions that glibc cannot be asked about (the numbers of the flags, struct stat, the dates of RISC OS files): answers worked out here */
static long slurp (int fd, char *b, long cap)                             /* the whole file of FD from the start */
{
  long total = 0;
  int r;
  if (lseek (fd, 0, SEEK_SET) != 0) return -1;
  while (total < cap && (r = (int) read (fd, b + total, (size_t) (cap - total))) > 0) total += r;
  return total;
}
static void test_g2self (void)
{
  const char *name = fsname (3), *name2 = fsname (2);
  char b[400];
  sect_begin ("g2self", 1);
  remove (name); remove (name2);
  /* the descriptor of a stream, and the stream of a descriptor */
  { FILE *f = fopen (name, "w+"); int fd = f ? fileno (f) : -1;
    check (f != 0 && fd >= 3 && fileno (f) == fd, "fileno: a stream gets a descriptor of 3 or more, and keeps it");
    check (f != 0 && fdopen (fd, "r+") == f && fdopen (fd, "w") == f, "fdopen: the stream of the descriptor, for a mode that fits");
    if (f)
      {
	fputs ("xyz", f);                                                  /* in the stdio buffer ... */
	check (write (fd, "123", 3) == 3, "write: a descriptor and a stream on one file");     /* ... and written after it, not before */
	rewind (f);
	check (fgets (b, sizeof b, f) != 0 && streq (b, "xyz123"), "write: what the stream had buffered is written first");
	check (lseek (fd, 1, SEEK_SET) == 1 && read (fd, b, 2) == 2 && b[0] == 'y' && b[1] == 'z' && fgetc (f) == '1', "read: after lseek the stream goes on where the descriptor stopped");
	fclose (f);
	T_ERRNO = 0; check (write (fd, "x", 1) == -1 && T_ERRNO == EBADF, "fclose closes the descriptor");
      } }
  { FILE *f = fopen (name, "r");
    int fd = f ? fileno (f) : -1;
    T_ERRNO = 0;
    check (f != 0 && fdopen (fd, "w") == 0 && T_ERRNO == EINVAL && fdopen (fd, "r+") == 0 && fdopen (fd, "x") == 0, "fdopen: a mode that does not fit is EINVAL");
    T_ERRNO = 0; check (f != 0 && write (fd, "x", 1) == -1 && T_ERRNO == EBADF, "write to a descriptor that is read only is EBADF");
    if (f) fclose (f); }
  T_ERRNO = 0; check (fdopen (99, "r") == 0 && T_ERRNO == EBADF && fileno (0) == -1, "fdopen of a descriptor that is not open is EBADF");
  /* open, fstat, fsync-free reading and writing */
  { int fd = open (name, O_RDWR | O_CREAT | O_TRUNC, 0644); struct stat st; struct stat64 st64;
    check (fd >= 3, "open: O_CREAT | O_TRUNC");
    check (write (fd, "hello, world", 12) == 12 && fstat (fd, &st) == 0 && st.st_size == 12 && S_ISREG (st.st_mode) && (st.st_mode & 0600) == 0600 && st.st_nlink == 1, "fstat: size 12, a regular file, read and write");
    check (fstat64 (fd, &st64) == 0 && st64.st_size == 12 && st64.st_mtime == st.st_mtime, "fstat64: the same");
    check (st.st_mtime > 1000000000, "fstat: the modification time is the date stamp of the file (after 2001: on the interpreter it is the host's clock, not the model's)");
    T_ERRNO = 0; check (fstat (99, &st) == -1 && T_ERRNO == EBADF, "fstat of a descriptor that is not open is EBADF");
    check (fstat (0, &st) == 0 && S_ISCHR (st.st_mode) && fstat (2, &st) == 0 && S_ISCHR (st.st_mode), "fstat of stdin and stderr: a character device");
    { int n = -1; check (lseek (fd, 3, SEEK_SET) == 3 && ioctl (fd, FIONREAD, &n) == 0 && n == 9, "ioctl FIONREAD: the bytes left in a file"); }
    T_ERRNO = 0; check (ioctl (fd, 0x12345678, 0) == -1 && T_ERRNO == ENOTTY, "ioctl: anything else is ENOTTY");
    T_ERRNO = 0; check (isatty (fd) == 0 && T_ERRNO == ENOTTY && isatty (0) == 1 && isatty (1) == 1 && isatty (2) == 1, "isatty: the standard streams yes, a file no (ENOTTY)");
    T_ERRNO = 0; check (isatty (99) == 0 && T_ERRNO == EBADF, "isatty of a descriptor that is not open is EBADF");
    { struct iovec iv[3] = { { (void *) "ab", 2 }, { (void *) "cde", 3 }, { (void *) "f", 1 } };
      check (lseek (fd, 0, SEEK_END) == 12 && writev (fd, iv, 3) == 6 && slurp (fd, b, sizeof b) == 18 && !strncmp (b, "hello, worldabcdef", 18), "writev: three blocks"); }
    { char r1[5], r2[20]; struct iovec iv[2] = { { r1, 5 }, { r2, 20 } };
      check (lseek (fd, 7, SEEK_SET) == 7 && readv (fd, iv, 2) == 11 && !strncmp (r1, "world", 5) && !strncmp (r2, "abcdef", 6), "readv: two blocks, the end of the file is a short read"); }
    T_ERRNO = 0; check (readv (fd, 0, 0) == -1 && T_ERRNO == EINVAL, "readv: no blocks is EINVAL");
    check (ftruncate (fd, 5) == 0 && fstat (fd, &st) == 0 && st.st_size == 5 && slurp (fd, b, sizeof b) == 5 && !strncmp (b, "hello", 5), "ftruncate: shorter");
    check (ftruncate (fd, 9) == 0 && slurp (fd, b, sizeof b) == 9 && !strncmp (b, "hello", 5) && b[5] == 0 && b[8] == 0, "ftruncate: longer, with zeros");
    check (close (fd) == 0, "close"); }
  { int fd = open (name, O_RDONLY); T_ERRNO = 0;
    check (fd >= 3 && ftruncate (fd, 2) == -1 && T_ERRNO == EINVAL, "ftruncate of a read only descriptor is EINVAL");
    if (fd >= 0) close (fd); }
  /* truncate and utime on the path */
  check (truncate (name, 3) == 0, "truncate: shorter");
  { int fd = open (name, O_RDONLY); struct stat st;
    check (fd >= 3 && fstat (fd, &st) == 0 && st.st_size == 3, "truncate: the file is 3 bytes");
    { struct utimbuf ub; ub.actime = ub.modtime = 1000000000;
      check (utime (name, &ub) == 0, "utime");
      check (fstat (fd, &st) == 0 && st.st_mtime == 1000000000, "utime: the modification time is 2001-09-09 01:46:40 (and fstat of the descriptor that open () made sees it)"); }
    { struct timeval tv[2]; tv[0].tv_sec = tv[1].tv_sec = 1500000000; tv[0].tv_usec = tv[1].tv_usec = 0;
      check (utimes (name, tv) == 0 && fstat (fd, &st) == 0 && st.st_mtime == 1500000000, "utimes: 2017-07-14"); }
    check (utime (name, 0) == 0 && fstat (fd, &st) == 0 && st.st_mtime - time (0) >= -4 && st.st_mtime - time (0) <= 4, "utime (path, NULL) is now");
    if (fd >= 0) close (fd); }
  T_ERRNO = 0; check (truncate ("NoSuchFileG2", 0) == -1 && T_ERRNO == ENOENT && utime ("NoSuchFileG2", 0) == -1 && T_ERRNO == ENOENT, "truncate and utime of a file that is not there: ENOENT");
  /* open: the errors */
  T_ERRNO = 0; check (open ("NoSuchFileG2", O_RDONLY) == -1 && T_ERRNO == ENOENT, "open: a file that is not there is ENOENT");
  T_ERRNO = 0; check (open ("NoSuchFileG2", O_WRONLY | O_TRUNC) == -1 && T_ERRNO == ENOENT, "open: O_TRUNC without O_CREAT does not make the file");
  T_ERRNO = 0; check (open (name, O_WRONLY | O_CREAT | O_EXCL) == -1 && T_ERRNO == EEXIST, "open: O_CREAT | O_EXCL of a file that is there is EEXIST");
  T_ERRNO = 0; check (open (name, 3) == -1 && T_ERRNO == EINVAL, "open: access mode 3 is EINVAL");
  T_ERRNO = 0; check (open (0, O_RDONLY) == -1, "open (NULL)");
  { int fd = open (name2, O_WRONLY | O_CREAT | O_EXCL, 0600);
    check (fd >= 3 && obj_type (name2) == 1, "open: O_CREAT | O_EXCL makes a new file");
    if (fd >= 0) close (fd); }
  { int fd = creat (name2, 0644); check (fd >= 3 && write (fd, "abc", 3) == 3 && close (fd) == 0 && obj_type (name2) == 1, "creat"); }
  { int fd = open (name2, O_RDONLY | O_CREAT); check (fd >= 3, "open: O_RDONLY | O_CREAT of a file that is there"); if (fd >= 0) close (fd); }
  check (unlink (name2) == 0 && obj_type (name2) == 0, "unlink: the file is gone");
  T_ERRNO = 0; check (unlink (name2) == -1 && T_ERRNO == ENOENT, "unlink: a file that is not there is ENOENT");
  { char dir[240]; _kernel_osfile_block ob = { 0, 0, 0, 0 };
    t_cpy (dir, g_fsbase); t_cpy (dir + t_len (dir), "dg2");
    _kernel_osfile (8, dir, &ob);
    T_ERRNO = 0; check (obj_type (dir) == 2 && unlink (dir) == -1 && T_ERRNO == EISDIR && obj_type (dir) == 2, "unlink: a directory is EISDIR, and stays");
    T_ERRNO = 0; check (open (dir, O_RDONLY) == -1 && T_ERRNO == EISDIR, "open: a directory is EISDIR");
    remove (dir); }
  /* the table of descriptors */
  { int fds[70], n = 0, last = -1, bad = 0;
    for (int k = 0; k < 70; k++) { fds[k] = open (name, O_RDONLY); if (fds[k] >= 0) { n++; last = fds[k]; if (fds[k] != 3 + k) bad++; } else break; }
    check (!bad && n == 61 && last == 63, "open: the descriptors are the lowest free ones, 3 to 63");
    T_ERRNO = 0; check (open (name, O_RDONLY) == -1 && T_ERRNO == EMFILE, "open: the 62nd is EMFILE");
    check (close (fds[10]) == 0 && open (name, O_RDONLY) == fds[10], "close: the lowest free descriptor is used again");
    for (int k = 0; k < n; k++) close (fds[k]); }
  /* the *64 names */
  { int fd = open64 (name, O_RDWR | O_CREAT | O_TRUNC); FILE *f;
    check (fd >= 3 && write (fd, "0123456789", 10) == 10 && lseek64 (fd, 4, SEEK_SET) == 4 && lseek64 (fd, 0x100000000LL, SEEK_SET) == -1 && T_ERRNO == EOVERFLOW, "open64 and lseek64 (a position beyond 2 GB is EOVERFLOW)");
    if (fd >= 0) close (fd);
    f = fopen64 (name, "r");
    { fpos64_t p = -1;
      check (f != 0 && fseeko64 (f, 3, SEEK_SET) == 0 && ftello64 (f) == 3 && fgetpos64 (f, &p) == 0 && p == 3 && fsetpos64 (f, &p) == 0 && fgetc (f) == '3', "fopen64, fseeko64, ftello64, fgetpos64, fsetpos64"); }
    if (f) fclose (f);
    fd = creat64 (name, 0644); check (fd >= 3, "creat64"); if (fd >= 0) close (fd); }
  remove (name); remove (name2);
  sect_end ();
}
#endif

/* ======================================================================== iconv (against glibc's): the encodings, valid and invalid text, small output windows */
#if defined (T_HOSTLIB)
# define T_EILSEQ 90                                                                           /* (the number in the library's own errno.h: the test is compiled with the host's) */
#else
# define T_EILSEQ EILSEQ
#endif
static int cv_err (void) { return T_ERRNO == T_EILSEQ ? 1 : T_ERRNO == EINVAL ? 2 : T_ERRNO == E2BIG ? 3 : T_ERRNO == 0 ? 0 : 9; }
/* the test's own encoder (not the library's): the code point CP in encoding E (0 UTF-8, 1 ASCII, 2 Latin 1, 3 UTF-16LE, 4 UTF-16BE, 5 UTF-32LE, 6 UTF-32BE) */
static size_t cv_put (int e, unsigned long cp, unsigned char *o)
{
  switch (e)
    {
    case 0: if (cp < 0x80) { o[0] = (unsigned char) cp; return 1; } if (cp < 0x800) { o[0] = (unsigned char) (0xC0 | (cp >> 6)); o[1] = (unsigned char) (0x80 | (cp & 63)); return 2; }
	    if (cp < 0x10000) { o[0] = (unsigned char) (0xE0 | (cp >> 12)); o[1] = (unsigned char) (0x80 | ((cp >> 6) & 63)); o[2] = (unsigned char) (0x80 | (cp & 63)); return 3; }
	    o[0] = (unsigned char) (0xF0 | (cp >> 18)); o[1] = (unsigned char) (0x80 | ((cp >> 12) & 63)); o[2] = (unsigned char) (0x80 | ((cp >> 6) & 63)); o[3] = (unsigned char) (0x80 | (cp & 63)); return 4;
    case 1: case 2: o[0] = (unsigned char) cp; return 1;
    case 3: case 4:
      { unsigned u[2], n = cp < 0x10000 ? 1 : 2, k; if (n == 1) u[0] = (unsigned) cp; else { u[0] = 0xD800 + (unsigned) ((cp - 0x10000) >> 10); u[1] = 0xDC00 + (unsigned) ((cp - 0x10000) & 0x3FF); }
	for (k = 0; k < n; k++) { o[2 * k] = (unsigned char) (e == 3 ? u[k] : u[k] >> 8); o[2 * k + 1] = (unsigned char) (e == 3 ? u[k] >> 8 : u[k]); } return 2 * n; }
    default:
      if (e == 5) { o[0] = (unsigned char) cp; o[1] = (unsigned char) (cp >> 8); o[2] = (unsigned char) (cp >> 16); o[3] = (unsigned char) (cp >> 24); }
      else { o[3] = (unsigned char) cp; o[2] = (unsigned char) (cp >> 8); o[1] = (unsigned char) (cp >> 16); o[0] = (unsigned char) (cp >> 24); }
      return 4;
    }
}
static void test_iconv (void)
{
  static const char *const names[] = { "UTF-8", "ASCII", "ISO-8859-1", "UTF-16LE", "UTF-16BE", "UTF-32LE", "UTF-32BE", "latin1", "utf8", "US-ASCII" };         /* (UCS-4 and WCHAR_T are not here: in glibc they take any value to 0x7FFFFFFF, in the library the same range as UTF-32) */
  static const int enc_of[] = { 0, 1, 2, 3, 4, 5, 6, 2, 0, 1 };
  sect_begin ("iconv", 34);
  for (unsigned i = 0; i < N (800); i++)
    {
      unsigned fi = rr (10), ti = rr (10), kind = rr (5);
      unsigned char in[200], out[600];
      size_t inlen = 0, outlen = 0;
      iconv_t cd;
      case_begin (i);
      cd = iconv_open (names[ti], names[fi]);
      rec_i (fi); rec_i (ti); rec_i (kind);
      rec_i (cd != (iconv_t) -1);
      if (cd == (iconv_t) -1) { case_end (); continue; }
      if (kind == 0)                                                                            /* bytes at random: mostly not valid */
	{ inlen = rr (40); for (size_t k = 0; k < inlen; k++) in[k] = (unsigned char) (rr (3) ? rr (128) : rr (256)); }
      else                                                                                      /* valid text of the encoding; kind 2: one byte changed, kind 3: cut short at a place of its own */
	{
	  unsigned n = rr (24);
	  int e = enc_of[fi];
	  for (unsigned k = 0; k < n && inlen < 150; k++)
	    {
	      unsigned r = rr (10);
	      unsigned long cp = r < 5 ? rr (128) : r < 7 ? 128 + rr (128) : r < 9 ? (rr (2) ? 0x100 + rr (0xD700 - 0x100) : 0xE000 + rr (0x2000)) : 0x10000 + rr (0x100000);
	      if (e == 1) cp &= 0x7F; else if (e == 2) cp &= 0xFF;
	      inlen += cv_put (e, cp, in + inlen);
	    }
	  if (kind == 2 && inlen) in[rr ((unsigned) inlen)] = (unsigned char) rr (256);
	  if (kind == 3 && inlen) inlen -= 1 + rr ((unsigned) (inlen < 4 ? inlen : 4));
	}
      rec_i ((long long) inlen);
      {
	char *ip = (char *) in, *op;
	size_t il = inlen, ol, r;
	int stop = 0;
	for (int round = 0; round < 80 && !stop; round++)
	  {
	    size_t win = rr (3) == 0 ? 1 + rr (4) : 4 + rr (40);
	    if (outlen + win > sizeof out) win = sizeof out - outlen;
	    op = (char *) out + outlen; ol = win;
	    T_ERRNO = 0;
	    r = iconv (cd, &ip, &il, &op, &ol);
	    rec_i ((long long) (long) r); rec_i (cv_err ());
	    rec_i ((long long) (inlen - il)); rec_i ((long long) (win - ol));
	    outlen += win - ol;
	    if (r != (size_t) -1 || cv_err () != 3) stop = 1;
	  }
	rec_m (out, outlen);
	{ char *op2 = (char *) out; size_t ol2 = 16; rec_i ((long long) (long) iconv (cd, 0, 0, &op2, &ol2)); rec_i ((long long) (16 - ol2)); }      /* the reset: nothing to write */
      }
      rec_i (iconv_close (cd));
      case_end ();
    }
  T_ERRNO = 0; rec_i ((long long) (long) iconv_open ("KLINGON", "UTF-8") == -1); rec_i (cv_err () == 2);
  T_ERRNO = 0; rec_i ((long long) (long) iconv_open ("UTF-8", "EBCDIC-NOPE") == -1); rec_i (cv_err () == 2);
  sect_end ();
}

/* ======================================================================== complex.h: the functions against the values of Python's cmath, and the special values of Annex G (self-checks: glibc passes them too) */
#include "libtest-complex.inc"
#if defined (T_HOSTLIB)
# define CX_MUL mk___muldc3                                                                    /* (the compiler calls __muldc3, which is libgcc's on the host: the test calls the library's own) */
# define CX_DIV mk___divdc3
#else
# define CX_MUL __muldc3
# define CX_DIV __divdc3
#endif
extern double complex CX_MUL (double, double, double, double);
extern double complex CX_DIV (double, double, double, double);
typedef double complex (*cx_fn) (double complex);
static double cx_err (double got_r, double got_i, double er, double ei)                       /* the error relative to the size of the value (an absolute one for 0) */
{
  double d = hypot (got_r - er, got_i - ei), m = hypot (er, ei);
  return m > 0 ? d / m : d;
}
static void cx_table (const char *name, const struct cx_case *t, int n, cx_fn f, double tol)
{
  double worst = 0;
  int i, bad = 0;
  for (i = 0; i < n; i++)
    {
      double complex r = f (CMPLX (t[i].zr, t[i].zi));
      double e = cx_err (creal (r), cimag (r), t[i].er, t[i].ei);
      if (!(e <= tol)) bad++;
      if (e > worst || e != e) worst = e;
    }
  if (g_verbose) printf ("  complex %-7s worst relative error %.3g (tolerance %.1g)\n", name, worst, tol);
  check (!bad, name);
}
static void cx_pow_table (void)
{
  double worst = 0;
  int i, bad = 0;
  for (i = 0; i < 60; i++)
    {
      double complex r = cpow (CMPLX (cx_cpow[i].zr, cx_cpow[i].zi), CMPLX (cx_cpow[i].wr, cx_cpow[i].wi));
      double e = cx_err (creal (r), cimag (r), cx_cpow[i].er, cx_cpow[i].ei);
      if (!(e <= 2e-12)) bad++;
      if (e > worst || e != e) worst = e;
    }
  if (g_verbose) printf ("  complex cpow    worst relative error %.3g\n", worst);
  check (!bad, "cpow");
}
static int cx_same (double complex z, double re, double im)                                    /* the same value, the zeros with their signs */
{
  return (creal (z) == re || (re != re && creal (z) != creal (z))) && (cimag (z) == im || (im != im && cimag (z) != cimag (z)))
	 && (re != 0 || signbit (creal (z)) == signbit (re)) && (im != 0 || signbit (cimag (z)) == signbit (im));
}
static int cx_close (double complex z, double re, double im) { return cx_err (creal (z), cimag (z), re, im) < 1e-14; }
static void test_complex (void)
{
  volatile double zero = 0.0, one = 1.0, inf = INFINITY, nan = NAN;
  double complex z;
  sect_begin ("complex", 33);
  cx_table ("cexp", cx_cexp, 60, cexp, 1e-13);
  cx_table ("clog", cx_clog, 60, clog, 1e-13);
  cx_table ("csqrt", cx_csqrt, 60, csqrt, 1e-13);
  cx_table ("csin", cx_csin, 60, csin, 1e-12);
  cx_table ("ccos", cx_ccos, 60, ccos, 1e-12);
  cx_table ("ctan", cx_ctan, 60, ctan, 1e-12);
  cx_table ("csinh", cx_csinh, 60, csinh, 1e-12);
  cx_table ("ccosh", cx_ccosh, 60, ccosh, 1e-12);
  cx_table ("ctanh", cx_ctanh, 60, ctanh, 1e-12);
  cx_table ("casin", cx_casin, 60, casin, 1e-12);
  cx_table ("cacos", cx_cacos, 60, cacos, 1e-12);
  cx_table ("catan", cx_catan, 60, catan, 1e-12);
  cx_table ("casinh", cx_casinh, 60, casinh, 1e-12);
  cx_table ("cacosh", cx_cacosh, 60, cacosh, 1e-12);
  cx_table ("catanh", cx_catanh, 60, catanh, 1e-12);
  cx_pow_table ();
  { double worst = 0; int bad = 0; for (int i = 0; i < 60; i++) { double a = cabs (CMPLX (cx_cabs[i].zr, cx_cabs[i].zi)), g = carg (CMPLX (cx_cabs[i].zr, cx_cabs[i].zi));
      double e1 = fabs (a - cx_cabs[i].er) / cx_cabs[i].er, e2 = fabs (g - cx_cabs[i].ei) / (fabs (cx_cabs[i].ei) + 1e-300); if (e1 > 1e-15 || e2 > 4e-16 * 4) bad++; if (e1 > worst) worst = e1; }
    if (g_verbose) printf ("  complex cabs    worst relative error %.3g\n", worst); check (!bad, "cabs and carg"); }
  /* the parts, and the simple values */
  z = CMPLX (3.0, 4.0);
  check (creal (z) == 3.0 && cimag (z) == 4.0 && cabs (z) == 5.0 && cx_same (conj (z), 3.0, -4.0) && cx_same (conj (CMPLX (1.0, 0.0)), 1.0, -0.0), "creal, cimag, cabs and conj");
  check (crealf (CMPLXF (1.5f, 2.5f)) == 1.5f && cimagf (CMPLXF (1.5f, 2.5f)) == 2.5f && cabsf (CMPLXF (3.0f, 4.0f)) == 5.0f && creall (CMPLXL (1.0L, 2.0L)) == 1.0L && cabsl (CMPLXL (6.0L, 8.0L)) == 10.0L, "the float and long double names");
  check (cx_same (cproj (CMPLX (inf, -2.0)), INFINITY, -0.0) && cx_same (cproj (CMPLX (3.0, -inf)), INFINITY, -0.0) && cx_same (cproj (CMPLX (1.0, 2.0)), 1.0, 2.0), "cproj: infinities go to the one point at infinity");
  /* Annex G: signed zeros and infinities */
  check (cx_same (cexp (CMPLX (zero, zero)), 1.0, 0.0) && cx_same (cexp (CMPLX (zero, -zero)), 1.0, -0.0) && cx_same (cexp (CMPLX (inf, zero)), INFINITY, 0.0), "cexp of 0, 0 - 0i and +inf");
  check (cx_same (cexp (CMPLX (-inf, one)), 0.0 * cos (1.0), 0.0 * sin (1.0)), "cexp of -inf + i: zeros with the signs of cos 1 and sin 1");
  check (cx_same (csqrt (CMPLX (-4.0, zero)), 0.0, 2.0) && cx_same (csqrt (CMPLX (-4.0, -zero)), 0.0, -2.0) && cx_same (csqrt (CMPLX (zero, zero)), 0.0, 0.0) && cx_same (csqrt (CMPLX (4.0, zero)), 2.0, 0.0), "csqrt: the two sides of the cut, zero, a positive real");
  check (cx_same (csqrt (CMPLX (inf, 1.0)), INFINITY, 0.0) && cx_same (csqrt (CMPLX (-inf, 1.0)), 0.0, INFINITY) && cx_same (csqrt (CMPLX (1.0, -inf)), INFINITY, -INFINITY), "csqrt of the infinities");
  check (cx_close (csqrt (CMPLX (3.0, 4.0)), 2.0, 1.0) && cx_close (csqrt (CMPLX (-3.0, 4.0)), 1.0, 2.0), "csqrt: 3+4i is 2+i, -3+4i is 1+2i");
  check (cx_close (csqrt (CMPLX (1e308, 1e308)), 1.09868411346781e+154, 4.5508986056222734e+153) && cx_close (csqrt (CMPLX (1e-310, 1e-310)), 1.0986841134678082e-155, 4.550898605622267e-156), "csqrt of numbers near the limits: no overflow, no underflow");
  check (cx_same (clog (CMPLX (-1.0, zero)), 0.0, M_PI) && cx_same (clog (CMPLX (-1.0, -zero)), 0.0, -M_PI) && cx_same (clog (CMPLX (zero, zero)), -INFINITY, 0.0) && cx_same (clog (CMPLX (1.0, zero)), 0.0, 0.0), "clog: the two sides of the cut, zero, one");
  check (cx_close (clog (CMPLX (1.0 + 1e-9, 1e-9)), 1.000000082740371e-09, 9.99999999e-10), "clog of a number near 1 (the log1p form keeps the digits)");
  check (cx_same (csinh (CMPLX (zero, zero)), 0.0, 0.0) && cx_same (ccosh (CMPLX (zero, zero)), 1.0, 0.0) && cx_same (csin (CMPLX (zero, zero)), 0.0, 0.0) && cx_same (ctanh (CMPLX (inf, one)), 1.0, 0.0 * sin (2.0)), "csinh, ccosh, csin of 0, and ctanh of +inf");
  check (cx_close (casin (CMPLX (2.0, zero)), 1.5707963267948966, 1.3169578969248166) && cx_same (cacos (CMPLX (2.0, zero)), 0.0, -1.3169578969248166), "casin and cacos of 2 + 0i");
  check (creal (cacos (CMPLX (2.0, -zero))) == 0.0 && cx_close (cacos (CMPLX (2.0, -zero)), 0.0, 1.3169578969248166) && cimag (cacos (CMPLX (2.0, zero))) < -1.3, "cacos of 2 - 0i and 2 + 0i: the imaginary parts have opposite signs");
  check (cx_close (catanh (CMPLX (2.0, zero)), 0.5493061443340549, 1.5707963267948966) && cx_close (catanh (CMPLX (2.0, -zero)), 0.5493061443340549, -1.5707963267948966) && cx_close (catan (CMPLX (zero, 2.0)), 1.5707963267948966, 0.5493061443340549), "catanh of 2 + 0i and 2 - 0i, catan of 2i");
  check (cx_close (casinh (CMPLX (1e-5, 2e-5)), 1.0000000001833332e-05, 2.0000000000333336e-05), "casinh of a small number (the series)");
  check (cx_close (casinh (CMPLX (1e200, 1e200)), 461.55673936964905, 0.7853981633974483) && cx_close (cacosh (CMPLX (1e200, 1e200)), 461.55673936964905, 0.7853981633974484), "casinh and cacosh of a huge number: no overflow");
  check (cx_close (cpow (CMPLX (2.0, zero), CMPLX (10.0, zero)), 1024.0, 0.0) && cx_close (cpow (CMPLX (4.0, zero), CMPLX (0.5, zero)), 2.0, 0.0), "cpow: 2^10 and 4^0.5");
  { double complex r = cpow (CMPLX (zero, 1.0), CMPLX (2.0, zero)); check (fabs (creal (r) + 1.0) < 1e-14 && fabs (cimag (r)) < 1e-14, "cpow: i^2 = -1"); }
  /* the operators: __muldc3 and __divdc3 */
  z = CX_MUL (1.0, 2.0, 3.0, 4.0);
  check (cx_same (z, -5.0, 10.0), "multiplication: (1+2i)(3+4i) = -5+10i");
  z = CX_DIV (-5.0, 10.0, 3.0, 4.0);
  check (cx_close (z, 1.0, 2.0) && cx_close (CX_DIV (1.0, 0.0, 0.0, 1.0), 0.0, -1.0), "division: (-5+10i)/(3+4i) = 1+2i; 1/i = -i");
  z = CX_MUL (nan, inf, 1.0, zero);
  check (isnan (creal (z)) && isinf (cimag (z)), "multiplication: (NaN + i inf) * 1 keeps the infinity (the NaN recovery of Annex G)");
  z = CX_MUL (inf, 1.0, inf, -1.0);
  check (isinf (creal (z)) && creal (z) > 0, "multiplication: (inf + i)(inf - i): an infinite real part");
  z = CX_DIV (1.0, 1.0, zero, zero);
  check (isinf (creal (z)) && isinf (cimag (z)), "division by zero: infinities (not NaN)");
  z = CX_DIV (1e300, 1e300, 1e300, 1e300);
  check (cx_close (z, 1.0, 0.0), "division of huge numbers: scaled, no overflow");
  z = CX_DIV (1e-300, 2e-300, 3e-300, 4e-300);
  check (cx_close (z, 0.43999999999999995, 0.08), "division of tiny numbers: scaled, no underflow");
  z = CX_DIV (1.0, 2.0, inf, 1.0);
  check (creal (z) == 0 && cimag (z) == 0, "division by an infinity: zero");
  { volatile double complex a = CMPLX (1.0, 2.0), b = CMPLX (3.0, -1.0);                      /* and the operators themselves, through the compiler */
    double complex m = a * b, d = a / b;
    check (cx_close (m, 5.0, 5.0) && cx_close (d, 0.1, 0.7), "the operators * and / on complex numbers (through the compiler)"); }
  sect_end ();
}

/* ======================================================================== group 3: access, dup, dup2, fsync (against glibc's), and stat on the machine */
static void test_g3 (void)
{
  const char *name = fsname (0);
  sect_begin ("g3", 32);
  for (unsigned i = 0; i < N (300); i++)
    {
      int exists = rr (4) != 0, fd, d, r, t;
      static const int accs[] = { 0, 4, 2, 1, 6, 5, 7, 8, 12 };
      unsigned k, nops;
      case_begin (i);
      remove (name);
      if (exists) put_file (name, rr (3) == 0 ? rr (30) : rr (700));
      for (k = 0; k < sizeof accs / sizeof accs[0]; k++)                                       /* access: the modes F_OK, R_OK, W_OK, X_OK and some together; 8 and 12 are no mode: EINVAL */
	{
	  T_ERRNO = 0; r = access (name, accs[k]); rec_i (r);
	  if (r < 0) rec_i (norm_err2 ());
	}
      T_ERRNO = 0; r = dup (4999); rec_i (r); rec_i (norm_err2 ());       /* a descriptor that is not open */
      T_ERRNO = 0; r = dup2 (4999, 40); rec_i (r); rec_i (norm_err2 ());
      T_ERRNO = 0; r = fsync (4999); rec_i (r); rec_i (norm_err2 ());
      T_ERRNO = 0; r = dup2 (-1, 40); rec_i (r); rec_i (norm_err2 ());
      if (!exists) { case_end (); continue; }
      fd = open (name, 2);                                                  /* (2 is O_RDWR in both libraries) */
      rec_i (fd >= 3);
      if (fd < 0) { rec_i (unlink (name)); case_end (); continue; }
      d = dup (fd);
      rec_i (d >= 3 && d != fd);
      t = 24 + (int) rr (14);
      nops = 1 + rr (12);
      for (k = 0; k < nops; k++)
	{
	  unsigned op = rr (7);
	  int use = rr (3) == 0 ? t : rr (2) ? fd : d;
	  rec_i (-2000 - (long long) op);
	  if (use == t && t >= 0 && k == 0 && op != 5) { int r2; T_ERRNO = 0; r2 = dup2 (fd, t); rec_i (r2 == t); t = -t; use = d; }          /* dup2 to a number of our own, once */
	  if (use < 0) use = -use;
	  switch (op)
	    {
	    case 0: case 6:
	      { unsigned char b[200]; unsigned n = rr (3) == 0 ? rr (150) : rr (20); int w; rtext (b, n); T_ERRNO = 0; w = (int) write (use, b, n); rec_i (w); if (w < 0) rec_i (norm_err2 ()); break; }
	    case 1:
	      { unsigned char b[200]; unsigned n = rr (60); int g; t_set (b, 0xA5, sizeof b); T_ERRNO = 0; g = (int) read (use, b, n); rec_i (g); if (g > 0) rec_m (b, (size_t) g); else if (g < 0) rec_i (norm_err2 ()); break; }
	    case 2: { long off = (long) rr (300) - 20; T_ERRNO = 0; rec_i ((long long) (long) lseek (use, (off_t) off, SEEK_SET)); break; }
	    case 3: T_ERRNO = 0; rec_i (fsync (use)); break;
	    case 4: T_ERRNO = 0; rec_i (fdatasync (use)); break;
	    default:                                                             /* the position is shared: both numbers ask for the same one */
	      rec_i ((long long) (long) lseek (fd, 0, SEEK_CUR)); rec_i ((long long) (long) lseek (d, 0, SEEK_CUR)); break;
	    }
	}
      if (t < 0)                                                              /* the copy that dup2 made */
	{
	  T_ERRNO = 0; rec_i (dup2 (-t, -t)); rec_i (close (-t));
	  T_ERRNO = 0; rec_i (close (-t)); rec_i (norm_err2 ());
	}
      if (rr (2)) { rec_i (close (fd)); rec_i ((int) write (d, "end", 3)); rec_i (close (d)); }
      else { rec_i (close (d)); rec_i ((int) write (fd, "end", 3)); rec_i (close (fd)); }
      T_ERRNO = 0; rec_i (close (fd)); rec_i (norm_err2 ());
      T_ERRNO = 0; rec_i (close (d)); rec_i (norm_err2 ());
      T_ERRNO = 0; rec_i (fsync (fd)); rec_i (norm_err2 ());
      rec_file (name);
      rec_i (unlink (name));
      case_end ();
    }
  sect_end ();
}
#if defined (T_ARM)
static void test_g3self (void)
{
  const char *name = fsname (0), *name2 = fsname (1);
  char dirn[220];
  struct stat st; struct stat64 st64;
  int fd, fd2, d, i;
  _kernel_osfile_block ob = { 0, 0, 0, 0 };
  sect_begin ("g3self", 1);
  t_cpy (dirn, g_fsbase); t_cpy (dirn + t_len (dirn), "dg3");                   /* a directory of our own: the base is the start of a file name (on the machine: <scrap directory>.MK) */
  remove (dirn); remove (name); remove (name2);
  _kernel_osfile (8, dirn, &ob);
  T_ERRNO = 0; check (stat (name, &st) == -1 && T_ERRNO == ENOENT && lstat (name, &st) == -1 && T_ERRNO == ENOENT && access (name, F_OK) == -1 && T_ERRNO == ENOENT, "stat, lstat and access of a file that is not there: ENOENT");
  fd = open (name, O_RDWR | O_CREAT | O_TRUNC);
  check (fd >= 3 && write (fd, "0123456789", 10) == 10 && close (fd) == 0, "a file of ten bytes is made");
  check (stat (name, &st) == 0 && S_ISREG (st.st_mode) && st.st_size == 10 && st.st_nlink == 1 && (st.st_mode & 0600) == 0600 && st.st_blocks == 1 && st.st_blksize == 512 && st.st_mtime > 1000000000, "stat of a file: regular, ten bytes, one link, owner read and write, one block, a date");
  check (lstat (name, &st) == 0 && st.st_size == 10 && stat64 (name, &st64) == 0 && st64.st_size == 10 && lstat64 (name, &st64) == 0 && S_ISREG (st64.st_mode), "lstat, stat64 and lstat64 say the same");
  fd = open (name, O_RDONLY);
  { struct stat sf; check (fd >= 3 && fstat (fd, &sf) == 0 && sf.st_size == st.st_size && sf.st_mode == (st.st_mode & ~0222u | (sf.st_mode & 0222u)) && S_ISREG (sf.st_mode), "fstat and stat agree on a file"); }
  close (fd);
  check (stat (dirn, &st) == 0 && S_ISDIR (st.st_mode) && (st.st_mode & 0700) == 0700 && st.st_nlink == 2 && lstat (dirn, &st) == 0 && S_ISDIR (st.st_mode), "stat of a directory: S_IFDIR, the owner's three bits, two links");
  check (access (dirn, F_OK) == 0 && access (dirn, R_OK | W_OK | X_OK) == 0, "access of a directory: everything");
  check (access (name, F_OK) == 0 && access (name, R_OK) == 0 && access (name, W_OK) == 0 && access (name, R_OK | W_OK) == 0, "access of a file: F_OK, R_OK, W_OK");
  T_ERRNO = 0; check (access (name, X_OK) == -1 && T_ERRNO == EACCES, "access X_OK of a Text file: EACCES (only a file that is not Text or Data is executable)");
  T_ERRNO = 0; check (access (name, 8) == -1 && T_ERRNO == EINVAL && access ("nothing-there-at-all", 8) == -1 && T_ERRNO == EINVAL, "access with a mode that is none of them: EINVAL, whether or not the file is there");
  { _kernel_osfile_block b = { 0, 0, 0, 1 };                                                      /* the attributes: read only */
    check (_kernel_osfile (4, name, &b) != _kernel_ERROR && stat (name, &st) == 0 && (st.st_mode & 0222) == 0 && (st.st_mode & 0444) == 0444, "a file with the read-only attribute: no write bits");
    T_ERRNO = 0; check (access (name, W_OK) == -1 && T_ERRNO == EACCES && access (name, R_OK) == 0, "access W_OK of a read-only file: EACCES");
    b.end = 3; check (_kernel_osfile (4, name, &b) != _kernel_ERROR && access (name, W_OK) == 0, "and read and write again"); }
  /* dup and dup2 */
  fd = open (name, O_RDWR);
  d = dup (fd);
  check (fd >= 3 && d >= 3 && d != fd, "dup gives another descriptor");
  check (write (d, "abc", 3) == 3 && lseek (fd, 0, SEEK_CUR) == 3 && lseek (d, 0, SEEK_CUR) == 3, "the position is shared: a write through the copy moves the original");
  check (close (fd) == 0 && write (d, "def", 3) == 3 && lseek (d, 0, SEEK_CUR) == 6, "closing the original leaves the copy open and usable");
  T_ERRNO = 0; check (close (fd) == -1 && T_ERRNO == EBADF && write (fd, "x", 1) == -1 && T_ERRNO == EBADF, "the original is closed");
  check (close (d) == 0, "and the copy closes");
  T_ERRNO = 0; check (close (d) == -1 && T_ERRNO == EBADF, "closed twice: EBADF");
  d = dup (1);
  check (d >= 3 && close (d) == 0, "a copy of stdout, and it closes");
  check (fprintf (stdout, "") >= 0 && fflush (stdout) == 0 && write (1, "", 0) == 0, "stdout is still open after the copy was closed");
  fd = open (name, O_RDWR); fd2 = open (name2, O_RDWR | O_CREAT | O_TRUNC);
  check (fd >= 3 && fd2 >= 3 && dup2 (fd, fd2) == fd2 && dup2 (fd, fd) == fd, "dup2 onto an open descriptor: the old file is closed, and dup2 of a descriptor onto itself is itself");
  check (write (fd2, "ZZ", 2) == 2 && lseek (fd, 0, SEEK_CUR) == 2, "the new number is the first file, with the shared position");
  check (close (fd) == 0 && close (fd2) == 0, "both numbers close");
  T_ERRNO = 0; check (dup2 (1, 0) == -1 && T_ERRNO == ENOSYS && dup2 (fd, 99) == -1 && T_ERRNO == EBADF, "dup2 onto 0..2: ENOSYS (not supported); an unused or out of range number: EBADF");
  T_ERRNO = 0; check (dup2 (-1, 7) == -1 && T_ERRNO == EBADF && dup (-1) == -1 && T_ERRNO == EBADF, "a descriptor that is not open: EBADF");
  /* a lot of copies: EMFILE */
  fd = open (name, O_RDONLY);
  { int n = 0, last = -1; T_ERRNO = 0; while ((last = dup (fd)) >= 0 && n < 100) n++;
    check (last == -1 && T_ERRNO == EMFILE && n >= 55 && n <= 60, "dup until there is no number left: EMFILE");
    for (i = 3; i < 64; i++) if (i != fd) close (i); }
  check (close (fd) == 0, "and the descriptors close");
  /* fsync */
  fd = open (name, O_RDWR | O_TRUNC);
  check (fd >= 3 && write (fd, "abcd", 4) == 4 && stat (name, &st) == 0 && st.st_size == 4, "stat of a file that is open for writing gives its length, not the allocation");
  close (fd);
  fd = open (name, O_RDWR);
  check (fd >= 3 && write (fd, "sync", 4) == 4 && fsync (fd) == 0 && fdatasync (fd) == 0, "fsync and fdatasync of a file");
  close (fd);
  T_ERRNO = 0; check (fsync (fd) == -1 && T_ERRNO == EBADF && fsync (1) == 0 && fsync (0) == -1 && T_ERRNO == EINVAL, "fsync of a closed number: EBADF; of stdout: 0; of stdin: EINVAL");
  { FILE *f = fopen (name, "wb"); int n = fileno (f); check (f && n >= 3 && fwrite ("buffered", 1, 8, f) == 8 && fsync (n) == 0 && stat (name, &st) == 0 && st.st_size == 8, "fsync of a stream made with fopen writes what is buffered"); fclose (f); }
  remove (dirn); remove (name); remove (name2);
  sect_end ();
}
#endif

#include "libtest-fp.c"

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
  { const char *v = getenv ("LT_VERBOSE"); if (v && *v) g_verbose = 1; }                           /* (a system variable on the machine and the interpreter: the results of every case, as -v on the host) */
#if defined (T_HW)
  { const char *d = getenv ("Wimp$ScrapDir"); size_t l = d ? t_len (d) : 0; if (l > 150) l = 150; if (d) t_mov (g_fsbase, d, l); g_fsbase[l] = '.'; t_cpy (g_fsbase + l + 1, "MK");
    if (!d) { g_fail++; puts ("FAIL the system variable Wimp$ScrapDir is not set (run this in a Task window): the stdio files go to the current directory"); t_cpy (g_fsbase, "MK"); }
    puts ("stdio: the screen streams, to be SEEN: the next two lines come from fputs (stdout) and fprintf (stderr), then a fwrite and a printf");
    fputs ("  line 1 (fputs to stdout)\n", stdout); fprintf (stderr, "  line 2 (%s to stderr, %d)\n", "fprintf", 42); { static const char l3[] = "  line 3 (fwrite)\n"; fwrite (l3, 1, sizeof l3 - 1, stdout); } printf ("  line 4 (printf %s)\n", "ok"); }
#else
  t_cpy (g_fsbase, T_FSDIR);
#endif
  RUN ("ctype", test_ctype);
  RUN ("string", test_string);
  RUN ("numbers", test_numbers);
  RUN ("wchar", test_wchar);
#ifndef T_ORACLE
  RUN ("ldouble", test_ldouble);
#endif
  RUN ("sort", test_sort);
  RUN ("div", test_div);
  RUN ("sscanf", test_sscanf);
  RUN ("printf", test_printf);
  RUN ("stdio", test_stdio);
  RUN ("time", test_time);
  RUN ("limits", test_limits);
  RUN ("inttypes", test_inttypes);
  RUN ("signal", test_signal);
  RUN ("g1", test_g1);
  RUN ("fnmatch", test_fnmatch);
  RUN ("getline", test_getline);
  RUN ("fd", test_fd);
  RUN ("complex", test_complex);
  RUN ("iconv", test_iconv);
  RUN ("g3", test_g3);
  RUN ("c99", test_c99);
  RUN ("gamma", test_gamma);
#if defined (T_ARM)
  RUN ("g1self", test_g1self);                                                                          /* (before stdio3, which closes the standard streams) */
  RUN ("g2self", test_g2self);
  RUN ("g3self", test_g3self);
#endif
  RUN ("fparith", test_fparith);
  RUN ("fpconv", test_fpconv);
  RUN ("gccrt", test_gccrt);
  RUN ("fpprintf", test_fpprintf);
  RUN ("strtod", test_strtod);
  RUN ("fpscanf", test_fpscanf);
  RUN ("fpmathx", test_fpmathx);
  RUN ("fpmathsp", test_fpmathsp);
#if !defined (T_ORACLE)
  RUN ("fpmath", test_fpmath);
#endif
#if !defined (T_ORACLE)
  RUN ("rand", test_rand);
  RUN ("stdio2", test_stdio_misc);
#ifndef T_ORACLE
  RUN ("tmpfile", test_tmpfile);
#endif
  RUN ("probe", test_stdio_probe);
#endif
#if defined (T_HOSTLIB) || (defined (T_ARM) && !defined (T_HW))
  RUN ("swix", test_swix);
  RUN ("clock", test_clock);
  RUN ("stdio3", test_stdio_streams);
#if defined (T_ARM)
  RUN ("swixblock", test_swix_block);
#endif
#endif
#if defined (T_ARM)
  RUN ("heap", test_heap);
  RUN ("kernel", test_kernel);
  RUN ("arm", test_arm_only);
#endif
#if defined (T_HW)
  RUN ("hw", test_hw);
#endif
  { char b[40] = "TOTAL fail="; size_t n = t_len (b); char d[12]; int k = 0; unsigned v = g_fail; do d[k++] = (char) ('0' + v % 10); while (v /= 10); while (k) b[n++] = d[--k]; b[n] = 0; puts (b); }
#if defined (T_HW)
  return 0;                                                                                           /* (a return code that is not 0 is an error of *RMRun: the failures are in the text) */
#else
  return g_fail != 0;
#endif
}
