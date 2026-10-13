/* iconv.c - iconv_open, iconv and iconv_close (<iconv.h>): UTF-8, ASCII, ISO-8859-1, UTF-16 and UTF-32 in either byte order, any to any.  One character at a time: decode to a code point, encode it; a character that does not
   fit in the output stays in the input (E2BIG), one that is not valid is EILSEQ, one that is cut short is EINVAL, as in glibc. */
#pragma GCC optimize ("Os")
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <iconv.h>

enum { E_UTF8, E_ASCII, E_LATIN1, E_UTF16LE, E_UTF16BE, E_UTF32LE, E_UTF32BE };
struct conv { int from, to; };

static int lower (int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int name_is (const char *name, const char *want)                      /* case-insensitive, up to a "//" suffix */
{
  while (*want) { if (lower ((unsigned char) *name) != *want) return 0; name++; want++; }
  return *name == 0 || (name[0] == '/' && name[1] == '/');
}
static int lookup (const char *name)
{
  static const struct { const char *n; int e; } tab[] = {
    { "utf-8", E_UTF8 }, { "utf8", E_UTF8 }, { "ascii", E_ASCII }, { "us-ascii", E_ASCII }, { "ansi_x3.4-1968", E_ASCII }, { "iso-8859-1", E_LATIN1 }, { "iso8859-1", E_LATIN1 }, { "iso_8859-1", E_LATIN1 },
    { "latin1", E_LATIN1 }, { "l1", E_LATIN1 }, { "utf-16le", E_UTF16LE }, { "utf16le", E_UTF16LE }, { "utf-16be", E_UTF16BE }, { "utf16be", E_UTF16BE },
    { "utf-32le", E_UTF32LE }, { "utf32le", E_UTF32LE }, { "ucs-4le", E_UTF32LE }, { "wchar_t", E_UTF32LE }, { "utf-32be", E_UTF32BE }, { "utf32be", E_UTF32BE }, { "ucs-4be", E_UTF32BE } };
  unsigned i;
  for (i = 0; i < sizeof tab / sizeof tab[0]; i++) if (name_is (name, tab[i].n)) return tab[i].e;
  return -1;
}
iconv_t iconv_open (const char *tocode, const char *fromcode)
{
  struct conv *c;
  int t, f;
  if (!tocode || !fromcode) { errno = EINVAL; return (iconv_t) -1; }
  t = lookup (tocode); f = lookup (fromcode);
  if (t < 0 || f < 0) { errno = EINVAL; return (iconv_t) -1; }
  c = malloc (sizeof *c);
  if (!c) { errno = ENOMEM; return (iconv_t) -1; }
  c->from = f; c->to = t;
  return c;
}
int iconv_close (iconv_t cd)
{
  if (cd == (iconv_t) -1 || !cd) { errno = EBADF; return -1; }
  free (cd);
  return 0;
}

/* the code point at P (N bytes left): returns the length used, 0 when it is cut short (EINVAL), -1 when it is not valid (EILSEQ) */
static int decode (int e, const unsigned char *p, size_t n, unsigned long *cp)
{
  switch (e)
    {
    case E_ASCII: if (p[0] >= 0x80) return -1; *cp = p[0]; return 1;
    case E_LATIN1: *cp = p[0]; return 1;
    case E_UTF8:
      {
	unsigned c = p[0];
	if (c < 0x80) { *cp = c; return 1; }
	if (c < 0xC2 || c > 0xF4) return -1;
	if (c < 0xE0)
	  {
	    if (n < 2) return 0;
	    if ((p[1] & 0xC0) != 0x80) return -1;
	    *cp = ((c & 0x1F) << 6) | (p[1] & 0x3F); return 2;
	  }
	if (c < 0xF0)
	  {
	    if (n < 2) return 0;
	    if ((p[1] & 0xC0) != 0x80 || (c == 0xE0 && p[1] < 0xA0) || (c == 0xED && p[1] >= 0xA0)) return -1;
	    if (n < 3) return 0;
	    if ((p[2] & 0xC0) != 0x80) return -1;
	    *cp = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); return 3;
	  }
	if (n < 2) return 0;
	if ((p[1] & 0xC0) != 0x80 || (c == 0xF0 && p[1] < 0x90) || (c == 0xF4 && p[1] >= 0x90)) return -1;
	if (n < 3) return 0;
	if ((p[2] & 0xC0) != 0x80) return -1;
	if (n < 4) return 0;
	if ((p[3] & 0xC0) != 0x80) return -1;
	*cp = ((unsigned long) (c & 0x07) << 18) | ((unsigned long) (p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
	return 4;
      }
    case E_UTF16LE: case E_UTF16BE:
      {
	int le = e == E_UTF16LE;
	unsigned u, v;
	if (n < 2) return 0;
	u = le ? p[0] | (p[1] << 8) : (p[0] << 8) | p[1];
	if (u >= 0xDC00 && u < 0xE000) return -1;                           /* a low surrogate on its own */
	if (u < 0xD800 || u >= 0xE000) { *cp = u; return 2; }
	if (n < 4) return 0;
	v = le ? p[2] | (p[3] << 8) : (p[2] << 8) | p[3];
	if (v < 0xDC00 || v >= 0xE000) return -1;
	*cp = 0x10000 + (((unsigned long) (u - 0xD800) << 10) | (v - 0xDC00));
	return 4;
      }
    default:                                                                 /* UTF-32 */
      {
	unsigned long u;
	if (n < 4) return 0;
	u = e == E_UTF32LE ? (unsigned long) p[0] | ((unsigned long) p[1] << 8) | ((unsigned long) p[2] << 16) | ((unsigned long) p[3] << 24)
			   : ((unsigned long) p[0] << 24) | ((unsigned long) p[1] << 16) | ((unsigned long) p[2] << 8) | p[3];
	if (u > 0x10FFFF || (u >= 0xD800 && u < 0xE000)) return -1;
	*cp = u; return 4;
      }
    }
}
/* the bytes of code point CP in encoding E at OUT (ROOM left): the length, 0 when they do not fit (E2BIG), -1 when the encoding cannot have it (EILSEQ) */
static int encode (int e, unsigned long cp, unsigned char *out, size_t room)
{
  switch (e)
    {
    case E_ASCII: if (!room) return 0; if (cp > 0x7F) return -1; out[0] = (unsigned char) cp; return 1;                /* (room first: glibc says E2BIG before it looks at the character) */
    case E_LATIN1: if (!room) return 0; if (cp > 0xFF) return -1; out[0] = (unsigned char) cp; return 1;
    case E_UTF8:
      if (cp < 0x80) { if (room < 1) return 0; out[0] = (unsigned char) cp; return 1; }
      if (cp < 0x800) { if (room < 2) return 0; out[0] = (unsigned char) (0xC0 | (cp >> 6)); out[1] = (unsigned char) (0x80 | (cp & 0x3F)); return 2; }
      if (cp < 0x10000) { if (room < 3) return 0; out[0] = (unsigned char) (0xE0 | (cp >> 12)); out[1] = (unsigned char) (0x80 | ((cp >> 6) & 0x3F)); out[2] = (unsigned char) (0x80 | (cp & 0x3F)); return 3; }
      if (room < 4) return 0;
      out[0] = (unsigned char) (0xF0 | (cp >> 18)); out[1] = (unsigned char) (0x80 | ((cp >> 12) & 0x3F)); out[2] = (unsigned char) (0x80 | ((cp >> 6) & 0x3F)); out[3] = (unsigned char) (0x80 | (cp & 0x3F));
      return 4;
    case E_UTF16LE: case E_UTF16BE:
      {
	int le = e == E_UTF16LE;
	unsigned u[2], k, n = cp < 0x10000 ? 1 : 2;
	if (room < 2 * n) return 0;
	if (n == 1) u[0] = (unsigned) cp;
	else { u[0] = 0xD800 + (unsigned) ((cp - 0x10000) >> 10); u[1] = 0xDC00 + (unsigned) ((cp - 0x10000) & 0x3FF); }
	for (k = 0; k < n; k++) { out[2 * k] = (unsigned char) (le ? u[k] : u[k] >> 8); out[2 * k + 1] = (unsigned char) (le ? u[k] >> 8 : u[k]); }
	return (int) (2 * n);
      }
    default:
      if (room < 4) return 0;
      if (e == E_UTF32LE) { out[0] = (unsigned char) cp; out[1] = (unsigned char) (cp >> 8); out[2] = (unsigned char) (cp >> 16); out[3] = (unsigned char) (cp >> 24); }
      else { out[3] = (unsigned char) cp; out[2] = (unsigned char) (cp >> 8); out[1] = (unsigned char) (cp >> 16); out[0] = (unsigned char) (cp >> 24); }
      return 4;
    }
}
size_t iconv (iconv_t cd, char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft)
{
  struct conv *c = cd;
  const unsigned char *in;
  unsigned char *out;
  size_t inleft, outleft;
  if (cd == (iconv_t) -1 || !cd) { errno = EBADF; return (size_t) -1; }
  if (!inbuf || !*inbuf) return 0;                                           /* (the reset: there is no state) */
  in = (const unsigned char *) *inbuf; inleft = inbytesleft ? *inbytesleft : 0;
  out = outbuf && *outbuf ? (unsigned char *) *outbuf : 0; outleft = outbytesleft ? *outbytesleft : 0;
  while (inleft)
    {
      unsigned long cp;
      int used = decode (c->from, in, inleft, &cp), put;
      if (used == 0) { errno = EINVAL; break; }
      if (used < 0) { errno = EILSEQ; break; }
      put = encode (c->to, cp, out, outleft);
      if (put == 0) { errno = E2BIG; break; }
      if (put < 0) { errno = EILSEQ; break; }
      in += used; inleft -= (size_t) used; out += put; outleft -= (size_t) put;
    }
  *inbuf = (char *) in; if (inbytesleft) *inbytesleft = inleft;
  if (outbuf) *outbuf = (char *) out;
  if (outbytesleft) *outbytesleft = outleft;
  return inleft ? (size_t) -1 : 0;
}
