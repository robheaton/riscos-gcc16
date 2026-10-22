/* iconv_open, iconv, iconv_close, iconvctl-less: character set conversion for UnixLib 64.
   Self-contained (no Iconv module): UTF-8, UTF-16, UTF-32, UCS-2, UCS-4, ASCII, ISO-8859-1 and the 8-bit sets of iconv_tables.h (including RISC OS Latin1).
   Follows glibc: "//TRANSLIT" and "//IGNORE" suffixes; iconv returns the number of irreversible conversions; E2BIG, EILSEQ and EINVAL leave the pointers at the
   start of the character concerned.  UTF-16 and UTF-32 write a byte order mark first and read one (little endian without one), as glibc does on this host. */
#include <errno.h>
#include <iconv.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "iconv_tables.h"

enum kind { K_UTF8, K_ASCII, K_LATIN1, K_UTF16, K_UTF32, K_8BIT };

struct cd {
  enum kind k;
  int bits;		/* K_UTF16/K_UTF32: 16 or 32 (UCS-2 and UCS-4: 16 or 32 with ucs set) */
  int ucs;		/* UCS-2/UCS-4: no surrogates; UCS-4 is limited to 0x7FFFFFFF */
  int order;		/* 0 = byte order from the BOM (little endian without one), 1 = little endian, 2 = big endian, 3 = host (UCS-2, UCS-4: big endian in glibc) */
  int bom;		/* 1 if the name asks for a byte order mark (UTF-16, UTF-32) */
  const unsigned short *hi;
};

struct iconv_state {
  struct cd from, to;
  int translit, ignore;
  int in_order;		/* detected byte order of the input (UTF-16/32 without a fixed order) */
  int bom_done;		/* output byte order mark written */
};

static int norm (const char *s, char *out, size_t max, int *translit, int *ignore)
{
  size_t n = 0;
  *translit = *ignore = 0;
  for (; *s; s++)
    {
      if (s[0] == '/' && s[1] == '/')
	{
	  /* suffixes: TRANSLIT, IGNORE (and unknown ones, tolerated) */
	  char w[16]; size_t l = 0;
	  s += 2;
	  while (*s && *s != '/' && l < sizeof w - 1) w[l++] = toupper ((unsigned char) *s++);
	  w[l] = 0;
	  if (!strcmp (w, "TRANSLIT")) *translit = 1;
	  else if (!strcmp (w, "IGNORE")) *ignore = 1;
	  while (*s && *s != '/') s++;
	  if (!*s) break;
	  s--;
	  continue;
	}
      if (*s == '-' || *s == '_' || *s == ' ' || *s == '.' || *s == ':') continue;
      if (n + 1 < max) out[n++] = toupper ((unsigned char) *s);
    }
  out[n] = 0;
  return 0;
}

static int lookup (const char *name, struct cd *c)
{
  size_t i;
  memset (c, 0, sizeof *c);
  if (!strcmp (name, "UTF8")) { c->k = K_UTF8; return 0; }
  if (!strcmp (name, "ASCII") || !strcmp (name, "USASCII") || !strcmp (name, "ANSIX3.41968") || !strcmp (name, "ANSIX341968") || !strcmp (name, "646")
      || !strcmp (name, "ISO646US") || !strcmp (name, "ISO646") || !strcmp (name, "US")) { c->k = K_ASCII; return 0; }
  if (!strcmp (name, "ISO88591") || !strcmp (name, "LATIN1") || !strcmp (name, "L1") || !strcmp (name, "CP819") || !strcmp (name, "IBM819")
      || !strcmp (name, "ISO885911987") || !strcmp (name, "ISOIR100")) { c->k = K_LATIN1; return 0; }
  if (!strcmp (name, "UTF16")) { c->k = K_UTF16; c->bits = 16; c->bom = 1; return 0; }
  if (!strcmp (name, "UTF16LE")) { c->k = K_UTF16; c->bits = 16; c->order = 1; return 0; }
  if (!strcmp (name, "UTF16BE")) { c->k = K_UTF16; c->bits = 16; c->order = 2; return 0; }
  if (!strcmp (name, "UCS2") || !strcmp (name, "ISO106462") || !strcmp (name, "UCS2LE") || !strcmp (name, "UNICODELITTLE"))
    { c->k = K_UTF16; c->bits = 16; c->ucs = 1; c->order = !strcmp (name, "UCS2") || !strcmp (name, "ISO106462") ? 3 : 1; return 0; }
  if (!strcmp (name, "UCS2BE") || !strcmp (name, "UNICODEBIG")) { c->k = K_UTF16; c->bits = 16; c->ucs = 1; c->order = 2; return 0; }
  if (!strcmp (name, "UNICODE")) { c->k = K_UTF16; c->bits = 16; c->ucs = 1; c->order = 0; c->bom = 1; return 0; }
  if (!strcmp (name, "UTF32")) { c->k = K_UTF32; c->bits = 32; c->bom = 1; return 0; }
  if (!strcmp (name, "UTF32LE")) { c->k = K_UTF32; c->bits = 32; c->order = 1; return 0; }
  if (!strcmp (name, "UTF32BE")) { c->k = K_UTF32; c->bits = 32; c->order = 2; return 0; }
  if (!strcmp (name, "WCHART")) { c->k = K_UTF32; c->bits = 32; c->order = 1; return 0; }
  if (!strcmp (name, "UCS4") || !strcmp (name, "ISO10646") || !strcmp (name, "UCS4BE") || !strcmp (name, "10646") || !strcmp (name, "ISO10646UCS4") || !strcmp (name, "ISO10646/UCS4/") || !strcmp (name, "10646/UCS4/"))
    { c->k = K_UTF32; c->bits = 32; c->ucs = 1; c->order = 2; return 0; }
  if (!strcmp (name, "UCS4LE")) { c->k = K_UTF32; c->bits = 32; c->ucs = 1; c->order = 1; return 0; }
  for (i = 0; i < sizeof cs_list / sizeof cs_list[0]; i++)
    if (!strcmp (name, cs_list[i].name)) { c->k = K_8BIT; c->hi = cs_list[i].hi; return 0; }
  for (i = 0; i < sizeof cs_alias / sizeof cs_alias[0]; i++)
    if (!strcmp (name, cs_alias[i].alias))
      {
	size_t j;
	for (j = 0; j < sizeof cs_list / sizeof cs_list[0]; j++)
	  if (!strcmp (cs_alias[i].name, cs_list[j].name)) { c->k = K_8BIT; c->hi = cs_list[j].hi; return 0; }
      }
  return -1;
}

iconv_t iconv_open (const char *tocode, const char *fromcode)
{
  struct iconv_state *s;
  char a[64], b[64];
  int t1, i1, t2, i2;
  if (!tocode || !fromcode) { errno = EINVAL; return (iconv_t) -1; }
  /* "" and "char" and "locale" mean the character set of the locale */
  norm (tocode, a, sizeof a, &t1, &i1);
  norm (fromcode, b, sizeof b, &t2, &i2);
  if (!a[0] || !strcmp (a, "CHAR") || !strcmp (a, "LOCALE")) strcpy (a, MB_CUR_MAX > 1 ? "UTF8" : "ASCII");
  if (!b[0] || !strcmp (b, "CHAR") || !strcmp (b, "LOCALE")) strcpy (b, MB_CUR_MAX > 1 ? "UTF8" : "ASCII");
  s = calloc (1, sizeof *s);
  if (!s) return (iconv_t) -1;
  if (lookup (a, &s->to) || lookup (b, &s->from)) { free (s); errno = EINVAL; return (iconv_t) -1; }
  s->translit = t1; s->ignore = i1;
  return (iconv_t) s;
}

int iconv_close (iconv_t cd)
{
  if (cd == (iconv_t) -1 || !cd) { errno = EBADF; return -1; }
  free (cd);
  return 0;
}

/* Decoding.  Returns 0 and sets *cp and *used, -1 for an invalid sequence, -2 if the input ends inside a character. */
static int get16 (const unsigned char *p, int order) { return order == 2 ? (p[0] << 8) | p[1] : p[0] | (p[1] << 8); }
static uint32_t get32 (const unsigned char *p, int order) { return order == 2 ? ((uint32_t) p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3] : p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t) p[3] << 24); }

static int decode (struct iconv_state *s, const unsigned char *p, size_t n, uint32_t *cp, size_t *used)
{
  const struct cd *c = &s->from;
  switch (c->k)
    {
    case K_ASCII:
      if (p[0] >= 0x80) return -1;
      *cp = p[0]; *used = 1; return 0;
    case K_LATIN1:
      *cp = p[0]; *used = 1; return 0;
    case K_8BIT:
      if (p[0] < 0x80) *cp = p[0];
      else { unsigned v = c->hi[p[0] - 0x80]; if (v == 0xFFFF) return -1; *cp = v; }
      *used = 1; return 0;
    case K_UTF8:
      {
	unsigned char b = p[0]; size_t need, i; uint32_t v;
	if (b < 0x80) { *cp = b; *used = 1; return 0; }
	if (b < 0xC2 || b > 0xF4) return -1;
	need = b < 0xE0 ? 2 : b < 0xF0 ? 3 : 4;
	v = b & (need == 2 ? 0x1F : need == 3 ? 0x0F : 0x07);
	for (i = 1; i < need; i++)
	  {
	    if (i >= n) return -2;
	    if ((p[i] & 0xC0) != 0x80) return -1;
	    if (i == 1)
	      {
		/* reject overlong forms, surrogates and values above 0x10FFFF as early as possible */
		if (b == 0xE0 && p[1] < 0xA0) return -1;
		if (b == 0xED && p[1] > 0x9F) return -1;
		if (b == 0xF0 && p[1] < 0x90) return -1;
		if (b == 0xF4 && p[1] > 0x8F) return -1;
	      }
	    v = (v << 6) | (p[i] & 0x3F);
	  }
	*cp = v; *used = need; return 0;
      }
    case K_UTF16:
      {
	int order = c->order, bomlen = 0, u;
	if (order == 3) order = 1;
	if (order == 0)
	  {
	    /* byte order mark: the first character decides, then it is remembered */
	    if (s->in_order == 0)
	      {
		if (n < 2) return -2;
		if (p[0] == 0xFF && p[1] == 0xFE) { s->in_order = 1; *cp = 0; *used = 2; return 1; }
		if (p[0] == 0xFE && p[1] == 0xFF) { s->in_order = 2; *cp = 0; *used = 2; return 1; }
		s->in_order = 1;
	      }
	    order = s->in_order;
	  }
	(void) bomlen;
	if (n < 2) return -2;
	u = get16 (p, order);
	if (c->ucs)
	  {
	    if (u >= 0xD800 && u < 0xE000) return -1;
	    *cp = u; *used = 2; return 0;
	  }
	if (u >= 0xDC00 && u < 0xE000) return -1;
	if (u >= 0xD800 && u < 0xDC00)
	  {
	    int l;
	    if (n < 4) return -2;
	    l = get16 (p + 2, order);
	    if (l < 0xDC00 || l >= 0xE000) return -1;
	    *cp = 0x10000 + ((u - 0xD800) << 10) + (l - 0xDC00); *used = 4; return 0;
	  }
	*cp = u; *used = 2; return 0;
      }
    case K_UTF32:
      {
	int order = c->order;
	uint32_t v;
	if (order == 0 || order == 3)
	  {
	    if (s->in_order == 0)
	      {
		if (n < 4) return -2;
		if (p[0] == 0xFF && p[1] == 0xFE && p[2] == 0 && p[3] == 0) { s->in_order = 1; *cp = 0; *used = 4; return 1; }
		if (p[0] == 0 && p[1] == 0 && p[2] == 0xFE && p[3] == 0xFF) { s->in_order = 2; *cp = 0; *used = 4; return 1; }
		s->in_order = 1;
	      }
	    order = s->in_order;
	  }
	if (n < 4) return -2;
	v = get32 (p, order);
	if (v > (c->ucs ? 0x7FFFFFFFu : 0x10FFFFu) || (v >= 0xD800 && v < 0xE000)) return -1;
	*cp = v; *used = 4; return 0;
      }
    }
  return -1;
}

static void put16 (unsigned char *o, unsigned v, int order)
{
  if (order == 2) { o[0] = v >> 8; o[1] = v; } else { o[0] = v; o[1] = v >> 8; }
}
static void put32 (unsigned char *o, uint32_t v, int order)
{
  if (order == 2) { o[0] = v >> 24; o[1] = v >> 16; o[2] = v >> 8; o[3] = v; }
  else { o[0] = v; o[1] = v >> 8; o[2] = v >> 16; o[3] = v >> 24; }
}

/* Encoding into o (at most 8 bytes).  Returns the number of bytes, or 0 if cp cannot be represented. */
static size_t encode (struct iconv_state *s, uint32_t cp, unsigned char *o, int with_bom)
{
  const struct cd *c = &s->to;
  size_t n = 0;
  switch (c->k)
    {
    case K_ASCII: if (cp >= 0x80) return 0; o[0] = cp; return 1;
    case K_LATIN1: if (cp > 0xFF) return 0; o[0] = cp; return 1;
    case K_8BIT:
      if (cp < 0x80) { o[0] = cp; return 1; }
      {
	int i;
	if (cp > 0xFFFE) return 0;
	for (i = 0; i < 128; i++) if (c->hi[i] == cp) { o[0] = 0x80 + i; return 1; }
      }
      return 0;
    case K_UTF8:
      if (cp < 0x80) { o[0] = cp; return 1; }
      if (cp < 0x800) { o[0] = 0xC0 | (cp >> 6); o[1] = 0x80 | (cp & 0x3F); return 2; }
      if (cp >= 0xD800 && cp < 0xE000) return 0;
      if (cp < 0x10000) { o[0] = 0xE0 | (cp >> 12); o[1] = 0x80 | ((cp >> 6) & 0x3F); o[2] = 0x80 | (cp & 0x3F); return 3; }
      if (cp > 0x10FFFF) return 0;
      o[0] = 0xF0 | (cp >> 18); o[1] = 0x80 | ((cp >> 12) & 0x3F); o[2] = 0x80 | ((cp >> 6) & 0x3F); o[3] = 0x80 | (cp & 0x3F); return 4;
    case K_UTF16:
      {
	int order = c->order == 2 ? 2 : (c->order == 3 ? 1 : 1);
	if (cp >= 0xD800 && cp < 0xE000) return 0;
	if (with_bom && c->bom && !s->bom_done) { put16 (o, 0xFEFF, order); n = 2; }
	if (cp < 0x10000) { put16 (o + n, cp, order); return n + 2; }
	if (c->ucs || cp > 0x10FFFF) return 0;
	cp -= 0x10000;
	put16 (o + n, 0xD800 + (cp >> 10), order); put16 (o + n + 2, 0xDC00 + (cp & 0x3FF), order);
	return n + 4;
      }
    case K_UTF32:
      {
	int order = c->order == 2 ? 2 : 1;
	if (cp >= 0xD800 && cp < 0xE000) return 0;
	if (cp > (c->ucs ? 0x7FFFFFFFu : 0x10FFFFu)) return 0;
	if (with_bom && c->bom && !s->bom_done) { put32 (o, 0xFEFF, order); n = 4; }
	put32 (o + n, cp, order);
	return n + 4;
      }
    }
  return 0;
}

/* "//TRANSLIT": a replacement text (ASCII) for a character the target set lacks */
static const char latin1_base[] = "AAAAAAACEEEEIIIIDNOOOOO*OUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
static const char *translit_text (uint32_t cp, char *buf)
{
  switch (cp)
    {
    case 0xA0: case 0x2002: case 0x2003: case 0x2009: case 0x202F: return " ";
    case 0x2010: case 0x2011: case 0x2012: case 0x2013: case 0x2014: case 0x2212: return "-";
    case 0x2018: case 0x2019: case 0x201B: case 0x2032: return "'";
    case 0x201C: case 0x201D: case 0x201F: case 0x2033: return "\"";
    case 0x201A: return ",";
    case 0x201E: return ",,";
    case 0x2022: return "o";
    case 0x2026: return "...";
    case 0x2039: return "<";
    case 0x203A: return ">";
    case 0xAB: return "<<";
    case 0xBB: return ">>";
    case 0xA9: return "(C)";
    case 0xAE: return "(R)";
    case 0x2122: return "(TM)";
    case 0x20AC: return "EUR";
    case 0xA3: return "GBP";
    case 0xA5: return "JPY";
    case 0xDF: return "ss";
    case 0xC6: return "AE";
    case 0xE6: return "ae";
    case 0x152: return "OE";
    case 0x153: return "oe";
    case 0xD0: return "D";
    case 0xF0: return "d";
    case 0xDE: return "TH";
    case 0xFE: return "th";
    case 0x2190: return "<-";
    case 0x2192: return "->";
    case 0xFB01: return "fi";
    case 0xFB02: return "fl";
    default: break;
    }
  if (cp >= 0xC0 && cp <= 0xFF && latin1_base[cp - 0xC0] != '*') { buf[0] = latin1_base[cp - 0xC0]; buf[1] = 0; return buf; }
  if (cp >= 0x100 && cp <= 0x17F)
    {
      /* Latin Extended-A: the plain letter */
      static const char ext[] = "AaAaAaCcCcCcCcDdDdEeEeEeEeEeGgGgGgGgHhHhIiIiIiIiIiJjJjKkkLlLlLlLlLlNnNnNnnNnOoOoOoOoRrRrRrSsSsSsSsTtTtTtUuUuUuUuUuUuWwYyYZzZzZzs";
      if (cp - 0x100 < sizeof ext - 1) { buf[0] = ext[cp - 0x100]; buf[1] = 0; return buf; }
    }
  return "?";
}

size_t iconv (iconv_t cdv, char **inbuf, size_t *inleft, char **outbuf, size_t *outleft)
{
  struct iconv_state *s = cdv;
  size_t irrev = 0;
  if (!s || cdv == (iconv_t) -1) { errno = EBADF; return (size_t) -1; }
  if (!inbuf || !*inbuf)
    {
      /* reset (the output state is only the byte order mark: nothing to write) */
      s->in_order = 0; s->bom_done = 0;
      return 0;
    }
  while (*inleft > 0)
    {
      const unsigned char *ip = (const unsigned char *) *inbuf;
      unsigned char tmp[24], *t;
      uint32_t cp;
      size_t used = 0, len;
      int r = decode (s, ip, *inleft, &cp, &used);
      if (r == -2) { errno = EINVAL; return (size_t) -1; }
      if (r == -1)
	{
	  if (s->ignore) { (*inbuf)++; (*inleft)--; irrev++; continue; }
	  errno = EILSEQ; return (size_t) -1;
	}
      if (r == 1)
	{
	  /* a byte order mark of the input: consumed, no output */
	  *inbuf += used; *inleft -= used;
	  continue;
	}
      len = encode (s, cp, tmp, 1);
      if (len == 0)
	{
	  if (s->translit)
	    {
	      char b1[2]; const char *txt = translit_text (cp, b1);
	      size_t k, total = 0;
	      t = tmp;
	      for (k = 0; txt[k]; k++)
		{
		  size_t w = encode (s, (unsigned char) txt[k], t, total == 0);
		  if (!w) { total = 0; break; }
		  t += w; total += w;
		  if (total > sizeof tmp - 8) break;
		}
	      len = total;
	      if (len) irrev++;
	    }
	  if (len == 0)
	    {
	      if (s->ignore) { *inbuf += used; *inleft -= used; irrev++; continue; }
	      errno = EILSEQ; return (size_t) -1;
	    }
	}
      if (len > *outleft) { errno = E2BIG; return (size_t) -1; }
      memcpy (*outbuf, tmp, len);
      *outbuf += len; *outleft -= len;
      *inbuf += used; *inleft -= used;
      if (s->to.bom) s->bom_done = 1;
    }
  return irrev;
}

/* the names of the character sets, for "iconv -l": index 0, 1, 2 ... gives the name, then NULL */
const char *__ul64_iconv_name (int index)
{
  static const char *const base[] = { "UTF-8", "ASCII", "ISO-8859-1", "UTF-16", "UTF-16LE", "UTF-16BE", "UTF-32", "UTF-32LE", "UTF-32BE", "UCS-2", "UCS-2LE", "UCS-2BE", "UCS-4", "UCS-4LE", "WCHAR_T" };
  static char buf[32];
  size_t nbase = sizeof base / sizeof base[0];
  if (index < 0) return 0;
  if ((size_t) index < nbase) return base[index];
  index -= nbase;
  if ((size_t) index >= sizeof cs_list / sizeof cs_list[0]) return 0;
  /* the table names have no hyphens (ISO88592): put them back as ISO-8859-2, WINDOWS-1252, KOI8-R, CP437, RISCOS-LATIN1 */
  {
    const char *n = cs_list[index].name;
    if (!strncmp (n, "ISO8859", 7)) snprintf (buf, sizeof buf, "ISO-8859-%s", n + 7);
    else if (!strncmp (n, "WINDOWS", 7)) snprintf (buf, sizeof buf, "WINDOWS-%s", n + 7);
    else if (!strncmp (n, "KOI8", 4)) snprintf (buf, sizeof buf, "KOI8-%s", n + 4);
    else if (!strncmp (n, "RISCOS", 6)) return "RISCOS-LATIN1";
    else return n;
    return buf;
  }
}
