/* fnmatch.c - fnmatch (): shell wildcard matching (POSIX 1003.2 with the GNU flags FNM_LEADING_DIR and FNM_CASEFOLD), in the "C" locale: a character is a byte, [:alpha:] and the other classes are ASCII. */
#pragma GCC optimize ("Os")                       /* not a hot path: the smaller code is the better one in a module */
#include <ctype.h>
#include <fnmatch.h>
#include <string.h>

#define FOLD(c) (((flags) & FNM_CASEFOLD) && (c) >= 'A' && (c) <= 'Z' ? (c) + 32 : (c))

static int class_match (const char *name, size_t len, int c)
{
  if (len == 5 && !memcmp (name, "alnum", 5)) return isalnum (c) != 0;
  if (len == 5 && !memcmp (name, "alpha", 5)) return isalpha (c) != 0;
  if (len == 5 && !memcmp (name, "blank", 5)) return c == ' ' || c == '\t';
  if (len == 5 && !memcmp (name, "cntrl", 5)) return iscntrl (c) != 0;
  if (len == 5 && !memcmp (name, "digit", 5)) return isdigit (c) != 0;
  if (len == 5 && !memcmp (name, "graph", 5)) return isgraph (c) != 0;
  if (len == 5 && !memcmp (name, "lower", 5)) return islower (c) != 0;
  if (len == 5 && !memcmp (name, "print", 5)) return isprint (c) != 0;
  if (len == 5 && !memcmp (name, "punct", 5)) return ispunct (c) != 0;
  if (len == 5 && !memcmp (name, "space", 5)) return isspace (c) != 0;
  if (len == 5 && !memcmp (name, "upper", 5)) return isupper (c) != 0;
  if (len == 6 && !memcmp (name, "xdigit", 6)) return isxdigit (c) != 0;
  return 0;
}

/* One element of a bracket expression at *PP: a character (a backslash escapes, [.x.] and [=x=] name one), or a [:class:].  Returns the character, -1 for a class (CLS and CLEN say which), or -2 when the
   bracket is not terminated.  */
static int element (const char **pp, int flags, const char **cls, size_t *clen)
{
  const char *p = *pp;
  int c = (unsigned char) *p++;
  if (!c) return -2;
  if (c == '\\' && !(flags & FNM_NOESCAPE))
    {
      c = (unsigned char) *p++;
      if (!c) return -2;
    }
  else if (c == '[' && (*p == ':' || *p == '.' || *p == '='))
    {
      char kind = *p;
      const char *q = p + 1;
      while (*q && !(*q == kind && q[1] == ']')) q++;
      if (*q)
	{
	  if (kind == ':') { *cls = p + 1; *clen = (size_t) (q - (p + 1)); *pp = q + 2; return -1; }
	  if (q - (p + 1) == 1) { c = (unsigned char) p[1]; *pp = q + 2; return c; }       /* [.x.] and [=x=] with one character (the "C" locale has no more) */
	}
    }
  *pp = p;
  return c;
}

/* the bracket expression after "[" at P against the character C: 1 match, 0 no match, -1 not a bracket expression (no closing "]"): "[" is then an ordinary character */
static int bracket (const char *p, int c, int flags, const char **end)
{
  int negate = 0, matched = 0, first = 1;
  int c0 = c;                                                               /* (with FNM_CASEFOLD the name's character and the pattern's are both folded, but a class is tested with the character as it is, as in glibc: [[:upper:]] matches A and not a) */
  c = FOLD (c);
  if (*p == '!' || *p == '^') { negate = 1; p++; }
  for (;;)
    {
      const char *cls = 0;
      size_t clen = 0;
      int lo;
      if (*p == ']' && !first) { p++; break; }
      first = 0;
      lo = element (&p, flags, &cls, &clen);
      if (lo == -2) return -1;
      if (lo == -1)
	{
	  if (class_match (cls, clen, c0)) matched = 1;
	  continue;
	}
      if (*p == '-' && p[1] && p[1] != ']')
	{
	  const char *q = p + 1;
	  int hi = element (&q, flags, &cls, &clen);
	  if (hi == -2) return -1;
	  if (hi >= 0)
	    {
	      p = q;
	      if (FOLD (lo) <= c && c <= FOLD (hi)) matched = 1;
	      continue;
	    }
	}
      if (FOLD (lo) == c) matched = 1;
    }
  *end = p;
  return matched != negate;
}

static int leading (const char *s, const char *start, int flags) { return s == start || ((flags & FNM_PATHNAME) && s[-1] == '/'); }

static int fnm (const char *p, const char *s, const char *start, int flags)
{
  for (;;)
    {
      int c = (unsigned char) *p++;
      switch (c)
	{
	case 0:
	  if (!*s) return 0;
	  if ((flags & FNM_LEADING_DIR) && *s == '/') return 0;
	  return FNM_NOMATCH;
	case '?':
	  if (!*s || ((flags & FNM_PATHNAME) && *s == '/')) return FNM_NOMATCH;
	  if (*s == '.' && (flags & FNM_PERIOD) && leading (s, start, flags)) return FNM_NOMATCH;
	  s++;
	  break;
	case '\\':
	  if (!(flags & FNM_NOESCAPE))
	    {
	      c = (unsigned char) *p++;
	      if (!c) return FNM_NOMATCH;                                   /* a trailing backslash loses */
	    }
	  goto literal;
	case '*':
	  if (*s == '.' && (flags & FNM_PERIOD) && leading (s, start, flags)) return FNM_NOMATCH;
	  for (c = (unsigned char) *p; c == '*' || c == '?'; c = (unsigned char) *++p)
	    if (c == '?')
	      {
		if (!*s || ((flags & FNM_PATHNAME) && *s == '/')) return FNM_NOMATCH;
		s++;
	      }
	  if (!c)
	    {
	      if (!(flags & FNM_PATHNAME) || (flags & FNM_LEADING_DIR)) return 0;
	      return strchr (s, '/') ? FNM_NOMATCH : 0;
	    }
	  for (;; s++)
	    {
	      if (!fnm (p, s, start, flags)) return 0;
	      if (!*s || ((flags & FNM_PATHNAME) && *s == '/')) return FNM_NOMATCH;
	    }
	case '[':
	  {
	    const char *end;
	    int r;
	    if (!*s || ((flags & FNM_PATHNAME) && *s == '/')) return FNM_NOMATCH;
	    if (*s == '.' && (flags & FNM_PERIOD) && leading (s, start, flags)) return FNM_NOMATCH;
	    r = bracket (p, (unsigned char) *s, flags, &end);
	    if (r < 0) goto literal;                                          /* no closing "]": an ordinary "[" */
	    if (!r) return FNM_NOMATCH;
	    p = end; s++;
	    break;
	  }
	default:
	literal:
	  if (!*s) return FNM_NOMATCH;
	  if (FOLD (c) != FOLD ((unsigned char) *s)) return FNM_NOMATCH;
	  s++;
	  break;
	}
    }
}
int fnmatch (const char *pattern, const char *name, int flags) { return fnm (pattern, name, name, flags); }
