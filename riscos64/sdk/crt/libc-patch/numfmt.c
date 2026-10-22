/* numfmt.c - vfprintf in front of picolibc's (which becomes __pico_vfprintf in libc.a: target/build-target.sh, refresh-sdk.sh): the numbers of printf follow LC_NUMERIC as in glibc:
   the decimal point of the locale in %f %e %g %a, and with the ' flag (%'d %'i %'u %'f %'F %'g %'G) the thousands separator and grouping of the locale.  In the "C" locale and in the locales with a "."
   decimal point (and no ' in the format) nothing is done: the format goes straight to picolibc.  Formats with positional arguments (%1$d) are left to picolibc too.  The SDK of RISC OS 64. */
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

extern int __pico_vfprintf (FILE *, const char *, va_list);
extern int __ul64_numeric (const char **radix, const char **thousep, const char **grouping);

static int inside;		/* set while this file formats one value with snprintf (which comes back here) */

struct obuf { char *p; size_t len, cap; };

static int oput (struct obuf *o, const char *s, size_t n)
{
  if (o->len + n + 1 > o->cap)
    {
      size_t nc = (o->len + n + 1) * 2 + 64;
      char *np = realloc (o->p, nc);
      if (!np) return 0;
      o->p = np;
      o->cap = nc;
    }
  memcpy (o->p + o->len, s, n);
  o->len += n;
  o->p[o->len] = 0;
  return 1;
}

/* the first run of digits of s (after a sign or a space) with the thousands separators put in; grouping as in localeconv: sizes from the right, the last one repeats, CHAR_MAX ends the grouping */
static char *group (const char *s, const char *thousep, const char *grouping)
{
  size_t n = strlen (s), a = 0, b, ds, seplen = strlen (thousep), digits, i, t = 0, out, cnt = 0, gi = 0, size;
  char *r, *tmp;
  while (a < n && (s[a] < '0' || s[a] > '9')) a++;
  b = a;
  while (b < n && s[b] >= '0' && s[b] <= '9') b++;
  /* zeros in front of the number (from a precision: %'.8d of 1000 is 00001,000) are not grouped */
  ds = a;
  while (b - ds > 1 && s[ds] == '0') ds++;
  digits = b - ds;
  r = malloc (n + digits * seplen + 2);
  tmp = malloc (digits * (seplen + 1) + 2);
  if (!r || !tmp) { free (r); free (tmp); return 0; }
  size = (unsigned char) grouping[0];
  for (i = digits; i > 0; i--)
    {
      if (size && size != (unsigned char) CHAR_MAX && cnt == size)
	{
	  size_t k;
	  for (k = seplen; k > 0; k--) tmp[t++] = thousep[k - 1];		/* (the text is built backwards) */
	  cnt = 0;
	  if (grouping[gi + 1]) size = (unsigned char) grouping[++gi];
	}
      tmp[t++] = s[ds + i - 1];
      cnt++;
    }
  memcpy (r, s, ds);
  out = ds;
  while (t > 0) r[out++] = tmp[--t];
  memcpy (r + out, s + b, n - b);
  r[out + n - b] = 0;
  free (tmp);
  return r;
}

/* takes the argument of one conversion from ap and formats it with spec (no width, no ' flag): the malloc'd text */
static char *one (const char *spec, int conv, const char *lenmod, va_list *app)
{
  int islong = lenmod[0] == 'l' || lenmod[0] == 'j' || lenmod[0] == 'z' || lenmod[0] == 't' || lenmod[0] == 'q';
  size_t n;
  char *r;
  inside++;
#define FORMAT(type, arg) do { type v = (arg); n = snprintf (0, 0, spec, v); r = malloc (n + 1); if (r) snprintf (r, n + 1, spec, v); } while (0)
  switch (conv)
    {
    case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A':
      if (lenmod[0] == 'L') FORMAT (long double, va_arg (*app, long double)); else FORMAT (double, va_arg (*app, double));
      break;
    case 's': case 'S': case 'p':
      FORMAT (void *, va_arg (*app, void *));
      break;
    case 'c': case 'C':
      FORMAT (int, va_arg (*app, int));
      break;
    default:
      if (islong) FORMAT (long long, va_arg (*app, long long)); else FORMAT (int, va_arg (*app, int));
      break;
    }
  inside--;
  return r;
}

int vfprintf (FILE *f, const char *fmt, va_list ap)
{
  const char *radix, *thousep, *grouping, *p;
  struct obuf o = { 0, 0, 0 };
  int total;
  size_t wr;
  if (inside || !__ul64_numeric (&radix, &thousep, &grouping) || strchr (fmt, '$')) return __pico_vfprintf (f, fmt, ap);
  if (radix[0] == '.' && !radix[1] && !strchr (fmt, '\'')) return __pico_vfprintf (f, fmt, ap);
  for (p = fmt; *p; )
    {
      const char *q;
      char spec[64], flags[8], lenmod[4], *txt;
      int width = -1, prec = -1, left = 0, quote = 0, zero = 0, nf = 0, conv, isint, isfloat;
      size_t sl = 0, ll = 0, n, i;
      if (*p != '%')
	{
	  q = strchr (p, '%');
	  if (!q) q = p + strlen (p);
	  if (!oput (&o, p, q - p)) goto fail;
	  p = q;
	  continue;
	}
      q = p + 1;
      for (;; q++)
	{
	  if (*q == '-') left = 1;
	  else if (*q == '0') zero = 1;
	  else if (*q == '\'') quote = 1;
	  else if ((*q == '+' || *q == ' ' || *q == '#') && nf < 6) flags[nf++] = *q;
	  else if (*q != '+' && *q != ' ' && *q != '#') break;
	}
      if (*q == '*')
	{
	  width = va_arg (ap, int);
	  if (width < 0) { width = -width; left = 1; }
	  q++;
	}
      else if (*q >= '0' && *q <= '9')
	{
	  width = 0;
	  while (*q >= '0' && *q <= '9') width = width * 10 + (*q++ - '0');
	}
      if (*q == '.')
	{
	  q++;
	  prec = 0;
	  if (*q == '*') { prec = va_arg (ap, int); q++; if (prec < 0) prec = -1; }
	  else while (*q >= '0' && *q <= '9') prec = prec * 10 + (*q++ - '0');
	}
      while ((*q == 'h' || *q == 'l' || *q == 'L' || *q == 'j' || *q == 'z' || *q == 't' || *q == 'q') && ll < 2) lenmod[ll++] = *q++;
      lenmod[ll] = 0;
      conv = *q;
      if (!conv)
	{
	  if (!oput (&o, p, q - p)) goto fail;
	  break;
	}
      q++;
      if (conv == '%')
	{
	  if (!oput (&o, "%", 1)) goto fail;
	  p = q;
	  continue;
	}
      if (conv == 'n')
	{
	  if (lenmod[0] == 'l' || lenmod[0] == 'j' || lenmod[0] == 'z' || lenmod[0] == 't') *va_arg (ap, long long *) = o.len;
	  else *va_arg (ap, int *) = (int) o.len;
	  p = q;
	  continue;
	}
      isint = conv == 'd' || conv == 'i' || conv == 'u';
      isfloat = strchr ("fFeEgGaA", conv) != 0;
      if (!isint && !isfloat && !strchr ("oxXcCsSp", conv))
	{
	  if (!oput (&o, p, q - p)) goto fail;		/* not a conversion: the text as it is */
	  p = q;
	  continue;
	}
      /* the spec of one conversion without width, zero padding, - and ': sign flags, precision, length, conversion character */
      spec[sl++] = '%';
      for (i = 0; i < (size_t) nf; i++) spec[sl++] = flags[i];
      if (prec >= 0) sl += snprintf (spec + sl, sizeof spec - sl, ".%d", prec);
      memcpy (spec + sl, lenmod, ll);
      sl += ll;
      spec[sl++] = conv;
      spec[sl] = 0;
      txt = one (spec, conv, lenmod, &ap);
      if (!txt) goto fail;
      if (isfloat && (radix[0] != '.' || radix[1]))
	{
	  /* the decimal point of the locale */
	  char *dot = strchr (txt, '.');
	  if (dot)
	    {
	      size_t rl = strlen (radix);
	      char *nt = malloc (strlen (txt) + rl + 1);
	      if (nt)
		{
		  memcpy (nt, txt, dot - txt);
		  memcpy (nt + (dot - txt), radix, rl);
		  strcpy (nt + (dot - txt) + rl, dot + 1);
		  free (txt);
		  txt = nt;
		}
	    }
	}
      if (quote && (isint || isfloat) && thousep[0])
	{
	  char *g = 0;
	  if (isfloat)
	    {
	      /* the digits before the decimal point (or the exponent) */
	      const char *dot = strstr (txt, radix);
	      size_t cut = dot ? (size_t) (dot - txt) : strcspn (txt, "eE");
	      char *head = malloc (cut + 1), *gh;
	      if (head)
		{
		  memcpy (head, txt, cut);
		  head[cut] = 0;
		  gh = group (head, thousep, grouping);
		  free (head);
		  if (gh)
		    {
		      g = malloc (strlen (gh) + strlen (txt + cut) + 1);
		      if (g) { strcpy (g, gh); strcat (g, txt + cut); }
		      free (gh);
		    }
		}
	    }
	  else g = group (txt, thousep, grouping);
	  if (g) { free (txt); txt = g; }
	}
      n = strlen (txt);
      if (width > 0)
	{
	  size_t cols = n, padn;
	  /* glibc counts the width of an integer in bytes and that of a floating point number in characters (a multibyte thousands separator is one) */
	  if (isfloat) for (i = 0, cols = 0; i < n; i++) if (((unsigned char) txt[i] & 0xC0) != 0x80) cols++;
	  padn = (size_t) width > cols ? width - cols : 0;
	  if (padn && left)
	    {
	      if (!oput (&o, txt, n)) { free (txt); goto fail; }
	      while (padn--) if (!oput (&o, " ", 1)) { free (txt); goto fail; }
	      n = 0;
	    }
	  else if (padn && zero && (isfloat || ((isint || conv == 'o' || conv == 'x' || conv == 'X') && prec < 0)) && txt[0] != 'n' && txt[0] != 'i')
	    {
	      size_t sgn = (txt[0] == '-' || txt[0] == '+' || txt[0] == ' ') ? 1 : 0;
	      if (txt[sgn] == '0' && (txt[sgn + 1] == 'x' || txt[sgn + 1] == 'X') && strchr ("xXaA", conv)) sgn += 2;		/* zeros go after 0x */
	      if (!oput (&o, txt, sgn)) { free (txt); goto fail; }
	      while (padn--) if (!oput (&o, "0", 1)) { free (txt); goto fail; }
	      if (!oput (&o, txt + sgn, n - sgn)) { free (txt); goto fail; }
	      n = 0;
	    }
	  else while (padn--) if (!oput (&o, " ", 1)) { free (txt); goto fail; }
	}
      if (n && !oput (&o, txt, n)) { free (txt); goto fail; }
      free (txt);
      p = q;
    }
  wr = o.len;
  total = (int) wr;
  if (wr && fwrite (o.p, 1, wr, f) != wr && ferror (f)) total = -1;
  free (o.p);
  return total;
fail:
  free (o.p);
  return -1;
}
