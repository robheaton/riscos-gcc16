/* collate.c - strcoll, strxfrm, strcoll_l, strxfrm_l, wcscoll and wcsxfrm of UnixLib 64.  In the locales C, POSIX and C.UTF-8 the order is that of the bytes (strcmp); in every other locale it is the
   order of glibc's ISO 14651 table (collate_tab.h, made by tools/gen-collate-data.py), compared in four levels: 1 the base letters and digits, 2 the accents, 3 the case (lower case first),
   4 the position of the characters in the table.  Punctuation and symbols have no weight at the first three levels (as in this table of glibc: "file_1" and "file1" differ only at level 4).
   The characters of U+0000 to U+2FFF and the ligatures U+FB00 to U+FB4F are in the table; the CJK ideographs (U+4E00 to U+9FA5) sort after them by code point; any other character (Hangul,
   kana ...) is ignored.  The language specific orders of some locales (Swedish, Danish, Czech, Spanish "ch" ...) are not applied: every locale uses this order.
   The six functions are renamed __pico_<name> in libc.a (target/build-target.sh, refresh-sdk.sh).  The SDK of RISC OS 64. */
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>

#include "collate_tab.h"

extern int __ul64_collate_iso14651 (void);
extern int __pico_strcoll (const char *, const char *);

/* the key of a string is made of four parts with a byte of 1 between them; each weight is two bytes of 2..255 (base 254) and, for a character with no weight in the table, 0xF0 and three bytes of
   its code point.  strcmp of two keys is the collation order */
struct key { unsigned char *p; size_t len, cap; };

static void kput (struct key *k, unsigned c)
{
  if (k->len < k->cap) k->p[k->len] = c;
  k->len++;
}
static void kweight (struct key *k, unsigned v)
{
  kput (k, 2 + v / 254);
  kput (k, 2 + v % 254);
}
static void kcp (struct key *k, unsigned cp)
{
  kput (k, 2 + (cp / (254 * 254)) % 254);
  kput (k, 2 + (cp / 254) % 254);
  kput (k, 2 + cp % 254);
}

static int find (unsigned cp)
{
  int lo = 0, hi = COLL_NCHARS - 1;
  if (cp > 0xFFFF) return -1;
  while (lo <= hi)
    {
      int mid = (lo + hi) / 2;
      if (coll_cp[mid] == cp) return mid;
      if (coll_cp[mid] < cp) lo = mid + 1; else hi = mid - 1;
    }
  return -1;
}

/* the next character of a UTF-8 string (a byte that is not part of a character counts as U+DC00 + byte) */
static unsigned next_cp (const unsigned char **sp)
{
  const unsigned char *s = *sp;
  unsigned c = *s, cp;
  int n, i;
  if (c < 0x80) { *sp = s + 1; return c; }
  if (c >= 0xC2 && c < 0xE0) { n = 1; cp = c & 0x1F; }
  else if (c >= 0xE0 && c < 0xF0) { n = 2; cp = c & 0x0F; }
  else if (c >= 0xF0 && c < 0xF5) { n = 3; cp = c & 0x07; }
  else { *sp = s + 1; return 0xDC00 + c; }
  for (i = 1; i <= n; i++)
    {
      if ((s[i] & 0xC0) != 0x80) { *sp = s + 1; return 0xDC00 + c; }
      cp = (cp << 6) | (s[i] & 0x3F);
    }
  *sp = s + n + 1;
  return cp;
}

/* builds the key of s into k (counting the length if k->cap is smaller) */
static void make_key (const char *str, struct key *k)
{
  int level;
  k->len = 0;
  for (level = 0; level < 4; level++)
    {
      const unsigned char *s = (const unsigned char *) str;
      while (*s)
	{
	  unsigned cp = next_cp (&s);
	  int i = find (cp);
	  if (level == 3) { if (i >= 0) kweight (k, coll_rank[i]); }
	  else if (i < 0)
	    {
	      /* no weights: the CJK ideographs sort after everything by code point (glibc's HAN section); any other character is ignored at the first three levels, as in glibc */
	      if (level == 0 && cp >= 0x4E00 && cp <= 0x9FA5) { kput (k, 0xF0); kcp (k, cp); }
	    }
	  else
	    {
	      const unsigned short *w = coll_pool + coll_off[i];
	      unsigned n, j;
	      for (j = 0; j < (unsigned) level; j++) w += 1 + w[0];
	      for (n = 0; n < w[0]; n++) kweight (k, w[1 + n]);
	    }
	}
      if (level < 3) kput (k, 1);
    }
}

int strcoll (const char *a, const char *b)
{
  unsigned char ba[256], bb[256], *pa, *pb;
  struct key ka = { ba, 0, sizeof ba }, kb = { bb, 0, sizeof bb };
  int r;
  if (!__ul64_collate_iso14651 ()) return strcmp (a, b);
  make_key (a, &ka);
  make_key (b, &kb);
  pa = ba; pb = bb;
  if (ka.len + 1 > sizeof ba) { pa = malloc (ka.len + 1); if (!pa) return strcmp (a, b); ka.p = pa; ka.cap = ka.len + 1; make_key (a, &ka); }
  if (kb.len + 1 > sizeof bb) { pb = malloc (kb.len + 1); if (!pb) { if (pa != ba) free (pa); return strcmp (a, b); } kb.p = pb; kb.cap = kb.len + 1; make_key (b, &kb); }
  pa[ka.len] = 0; pb[kb.len] = 0;
  r = strcmp ((char *) pa, (char *) pb);
  if (pa != ba) free (pa);
  if (pb != bb) free (pb);
  return r;
}

int strcoll_l (const char *a, const char *b, locale_t loc)
{
  (void) loc;
  return strcoll (a, b);
}

size_t strxfrm (char *dst, const char *src, size_t n)
{
  struct key k;
  if (!__ul64_collate_iso14651 ())
    {
      size_t l = strlen (src);
      if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy (dst, src, c); dst[c] = 0; }
      return l;
    }
  k.p = (unsigned char *) dst;
  k.len = 0;
  k.cap = n ? n - 1 : 0;
  make_key (src, &k);
  if (n) dst[k.len < n - 1 ? k.len : n - 1] = 0;
  return k.len;
}

size_t strxfrm_l (char *dst, const char *src, size_t n, locale_t loc)
{
  (void) loc;
  return strxfrm (dst, src, n);
}

/* wide strings: converted to UTF-8 first */
static char *w2u (const wchar_t *w)
{
  size_t n = 0, i;
  char *r, *p;
  for (i = 0; w[i]; i++) n += 4;
  r = p = malloc (n + 1);
  if (!r) return 0;
  for (i = 0; w[i]; i++)
    {
      unsigned long c = (unsigned long) w[i];
      if (c < 0x80) *p++ = c;
      else if (c < 0x800) { *p++ = 0xC0 | (c >> 6); *p++ = 0x80 | (c & 0x3F); }
      else if (c < 0x10000) { *p++ = 0xE0 | (c >> 12); *p++ = 0x80 | ((c >> 6) & 0x3F); *p++ = 0x80 | (c & 0x3F); }
      else { *p++ = 0xF0 | ((c >> 18) & 7); *p++ = 0x80 | ((c >> 12) & 0x3F); *p++ = 0x80 | ((c >> 6) & 0x3F); *p++ = 0x80 | (c & 0x3F); }
    }
  *p = 0;
  return r;
}

int wcscoll (const wchar_t *a, const wchar_t *b)
{
  char *x = w2u (a), *y = w2u (b);
  int r;
  if (!x || !y) { free (x); free (y); return wcscmp (a, b); }
  r = strcoll (x, y);
  free (x); free (y);
  return r;
}

size_t wcsxfrm (wchar_t *dst, const wchar_t *src, size_t n)
{
  char *x = w2u (src);
  size_t l, i;
  if (!x) return 0;
  l = strxfrm (0, x, 0);
  if (n && dst)
    {
      char *buf = malloc (l + 1);
      if (buf)
	{
	  strxfrm (buf, x, l + 1);
	  for (i = 0; i < l && i < n - 1; i++) dst[i] = (unsigned char) buf[i];
	  dst[i < n ? i : n - 1] = 0;
	  free (buf);
	}
    }
  free (x);
  return l;
}
