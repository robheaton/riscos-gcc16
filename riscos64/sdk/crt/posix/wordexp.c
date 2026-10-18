/* wordexp.c - wordexp and wordfree: the shell word expansion of a string (quotes, backslash, ~ and ~user, $name, ${name}, ${#name}, ${name:-w} ${name-w} ${name:+w} ${name+w}, $(command) and `command`
   through popen (so through sh -c where there is an sh), splitting of the unquoted results at IFS, and file names for * ? [..] through glob).  No arithmetic ($((..)) is WRDE_SYNTAX), no ${name:=w} or
   ${name%w}.  The syntax characters | & ; < > ( ) { } unquoted are WRDE_BADCHAR, as the standard says.  The SDK of RISC OS 64. */
#include <glob.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wordexp.h>

struct sb { char *p; size_t n, cap; };

static int sb_add (struct sb *b, const char *s, size_t k)
{
  if (b->n + k + 1 > b->cap)
    {
      size_t c = b->cap ? b->cap * 2 : 64;
      char *p;
      while (c < b->n + k + 1) c *= 2;
      p = realloc (b->p, c);
      if (!p) return -1;
      b->p = p; b->cap = c;
    }
  memcpy (b->p + b->n, s, k);
  b->n += k;
  b->p[b->n] = 0;
  return 0;
}
static int sb_c (struct sb *b, char c) { return sb_add (b, &c, 1); }

struct ctx
{
  char **v; size_t n, cap;			/* the words so far */
  struct sb val, pat;				/* the word being built, and its pattern for glob */
  int inword, magic, flags;
};

static int push (struct ctx *x, char *s)
{
  if (x->n + 2 > x->cap)
    {
      size_t c = x->cap ? x->cap * 2 : 16;
      char **p = realloc (x->v, c * sizeof *p);
      if (!p) { free (s); return WRDE_NOSPACE; }
      x->v = p; x->cap = c;
    }
  x->v[x->n++] = s; x->v[x->n] = 0;
  return 0;
}

static int finish (struct ctx *x)
{
  int r = 0;
  if (!x->inword) return 0;
  if (x->magic)
    {
      glob_t g;
      size_t i;
      memset (&g, 0, sizeof g);
      if (glob (x->pat.p ? x->pat.p : "", GLOB_QUOTE | GLOB_NOCHECK, 0, &g) == 0)
	for (i = 0; i < (size_t) g.gl_pathc && !r; i++)
	  {
	    char *c = strdup (g.gl_pathv[i]);
	    r = c ? push (x, c) : WRDE_NOSPACE;
	  }
      else r = WRDE_NOSPACE;
      globfree (&g);
    }
  else
    {
      char *c = strdup (x->val.p ? x->val.p : "");
      r = c ? push (x, c) : WRDE_NOSPACE;
    }
  x->val.n = 0; if (x->val.p) x->val.p[0] = 0;
  x->pat.n = 0; if (x->pat.p) x->pat.p[0] = 0;
  x->inword = x->magic = 0;
  return r;
}

/* one literal character, quoted (nothing special in it for glob) */
static int lit (struct ctx *x, char c)
{
  x->inword = 1;
  if (sb_c (&x->val, c)) return WRDE_NOSPACE;
  if (strchr ("*?[]\\", c) && sb_c (&x->pat, '\\')) return WRDE_NOSPACE;
  return sb_c (&x->pat, c) ? WRDE_NOSPACE : 0;
}

/* the text of an expansion: quoted, or split at IFS and made into patterns */
static int put_text (struct ctx *x, const char *t, int quoted)
{
  const char *ifs = getenv ("IFS");
  int r;
  if (!ifs) ifs = " \t\n";
  for (; *t; t++)
    {
      if (quoted) { if ((r = lit (x, *t))) return r; }
      else if (strchr (ifs, *t) && *t) { if (*ifs && (r = finish (x))) return r; }
      else
	{
	  x->inword = 1;
	  if (strchr ("*?[", *t)) x->magic = 1;
	  if (sb_c (&x->val, *t) || sb_c (&x->pat, *t)) return WRDE_NOSPACE;
	}
    }
  return 0;
}

static int cmd_output (struct ctx *x, const char *cmd, size_t len, int quoted)
{
  struct sb out = { 0, 0, 0 };
  char *c = malloc (len + 1), buf[256];
  FILE *f;
  size_t k;
  int r;
  if (!c) return WRDE_NOSPACE;
  memcpy (c, cmd, len); c[len] = 0;
  f = popen (c, "r");
  free (c);
  if (!f) return WRDE_NOSPACE;
  while ((k = fread (buf, 1, sizeof buf, f)) > 0) if (sb_add (&out, buf, k)) { pclose (f); free (out.p); return WRDE_NOSPACE; }
  pclose (f);
  while (out.n && out.p[out.n - 1] == '\n') out.p[--out.n] = 0;
  r = put_text (x, out.p ? out.p : "", quoted);
  free (out.p);
  return r;
}

/* the end of a ( ) or ${ } or `..` that starts at s (just after the opener): its length, or -1 */
static long span (const char *s, char open, char close)
{
  int depth = 1;
  const char *p = s;
  for (; *p; p++)
    {
      if (*p == '\\' && p[1]) p++;
      else if (*p == '\'' && open != '`') { p++; while (*p && *p != '\'') p++; if (!*p) return -1; }
      else if (open != '`' && *p == open) depth++;
      else if (*p == close && --depth == 0) return p - s;
    }
  return -1;
}

static int dollar (struct ctx *x, const char **pp, int quoted)
{
  const char *p = *pp + 1;			/* after the $ */
  char name[128];
  const char *val, *word = 0;
  size_t k = 0, wlen = 0;
  int len_of = 0, colon = 0, op = 0;
  if (*p == '(')
    {
      long n;
      if (p[1] == '(') return WRDE_SYNTAX;
      if (x->flags & WRDE_NOCMD) return WRDE_CMDSUB;
      n = span (p + 1, '(', ')');
      if (n < 0) return WRDE_SYNTAX;
      *pp = p + 1 + n;
      return cmd_output (x, p + 1, (size_t) n, quoted);
    }
  if (*p == '{')
    {
      long n = span (p + 1, '{', '}');
      const char *e, *q = p + 1;
      if (n < 0) return WRDE_SYNTAX;
      e = q + n;
      if (*q == '#') { len_of = 1; q++; }
      while (q < e && (*q == '_' || (*q >= '0' && *q <= '9') || (*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z')) && k < sizeof name - 1) name[k++] = *q++;
      name[k] = 0;
      if (!k) return WRDE_SYNTAX;
      if (q < e)
	{
	  if (*q == ':') { colon = 1; q++; }
	  if (q < e && strchr ("-+", *q)) { op = *q++; word = q; wlen = (size_t) (e - q); }
	  else return WRDE_SYNTAX;
	}
      *pp = e;
    }
  else if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_')
    {
      while (((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_' || (*p >= '0' && *p <= '9')) && k < sizeof name - 1) name[k++] = *p++;
      name[k] = 0;
      *pp = p - 1;
    }
  else return lit (x, '$');			/* a lone $ is a $ */
  val = getenv (name);
  if (len_of)
    {
      char t[24];
      snprintf (t, sizeof t, "%lu", (unsigned long) (val ? strlen (val) : 0));
      return put_text (x, t, quoted);
    }
  if (op == '-' && (!val || (colon && !*val))) { char *w = malloc (wlen + 1); int r; if (!w) return WRDE_NOSPACE; memcpy (w, word, wlen); w[wlen] = 0; r = put_text (x, w, quoted); free (w); return r; }
  if (op == '+') { if (val && (!colon || *val)) { char *w = malloc (wlen + 1); int r; if (!w) return WRDE_NOSPACE; memcpy (w, word, wlen); w[wlen] = 0; r = put_text (x, w, quoted); free (w); return r; } return 0; }
  if (!val) { if (x->flags & WRDE_UNDEF) return WRDE_BADVAL; return 0; }
  return put_text (x, val, quoted);
}

static int tilde (struct ctx *x, const char **pp)
{
  const char *p = *pp + 1, *e = p;
  char user[64];
  const char *home = 0;
  while (*e && *e != '/' && *e != ' ' && *e != '\t' && *e != '\n' && *e != ':') e++;
  if (e == p) home = getenv ("HOME");
  else if ((size_t) (e - p) < sizeof user)
    {
      const struct passwd *pw;
      memcpy (user, p, (size_t) (e - p)); user[e - p] = 0;
      pw = getpwnam (user);
      if (pw) home = pw->pw_dir;
    }
  if (!home) return -1;				/* not expanded: the ~ is an ordinary character */
  *pp = e - 1;
  x->inword = 1;
  return put_text (x, home, 1);
}

int wordexp (const char *words, wordexp_t *we, int flags)
{
  struct ctx x;
  const char *p;
  size_t i, offs = (flags & WRDE_DOOFFS) ? we->we_offs : 0, old = 0;
  int r = 0;
  memset (&x, 0, sizeof x);
  x.flags = flags;
  if (flags & WRDE_REUSE) wordfree (we);
  if ((flags & WRDE_APPEND) && !(flags & WRDE_REUSE) && we->we_wordv) { x.v = we->we_wordv; old = we->we_wordc; }
  if (!x.v)
    {
      x.v = calloc (offs + 16, sizeof *x.v);
      if (!x.v) return WRDE_NOSPACE;
      x.cap = offs + 16; x.n = offs;
    }
  else { x.n = offs + old; x.cap = x.n + 1; }
  for (p = words; *p && !r; p++)
    {
      char c = *p;
      if (c == ' ' || c == '\t' || c == '\n') r = finish (&x);
      else if (strchr ("|&;<>(){}", c)) r = WRDE_BADCHAR;
      else if (c == '\\') { if (p[1]) { p++; r = lit (&x, *p); } }
      else if (c == '\'')
	{
	  x.inword = 1;
	  for (p++; *p && *p != '\'' && !r; p++) r = lit (&x, *p);
	  if (!*p) r = WRDE_SYNTAX;
	}
      else if (c == '"')
	{
	  x.inword = 1;
	  for (p++; *p && *p != '"' && !r; p++)
	    {
	      if (*p == '\\' && p[1] && strchr ("$`\"\\\n", p[1])) { p++; if (*p != '\n') r = lit (&x, *p); }
	      else if (*p == '$') r = dollar (&x, &p, 1);
	      else if (*p == '`')
		{
		  long n = span (p + 1, '`', '`');
		  if (n < 0) r = WRDE_SYNTAX;
		  else if (flags & WRDE_NOCMD) r = WRDE_CMDSUB;
		  else { r = cmd_output (&x, p + 1, (size_t) n, 1); p += n + 1; }
		}
	      else r = lit (&x, *p);
	    }
	  if (!r && !*p) r = WRDE_SYNTAX;
	}
      else if (c == '$') r = dollar (&x, &p, 0);
      else if (c == '`')
	{
	  long n = span (p + 1, '`', '`');
	  if (n < 0) r = WRDE_SYNTAX;
	  else if (flags & WRDE_NOCMD) r = WRDE_CMDSUB;
	  else { r = cmd_output (&x, p + 1, (size_t) n, 0); p += n + 1; }
	}
      else if (c == '~' && !x.inword)
	{
	  int t = tilde (&x, &p);
	  if (t < 0) r = lit (&x, c);
	  else r = t;
	}
      else
	{
	  x.inword = 1;
	  if (strchr ("*?[", c)) x.magic = 1;
	  if (sb_c (&x.val, c) || sb_c (&x.pat, c)) r = WRDE_NOSPACE;
	}
    }
  if (!r) r = finish (&x);
  free (x.val.p); free (x.pat.p);
  if (r)
    {
      for (i = offs + old; i < x.n; i++) free (x.v[i]);
      if (!old) free (x.v);
      else x.v[offs + old] = 0;
      return r;
    }
  we->we_wordv = x.v;
  we->we_wordc = x.n - offs;
  we->we_offs = offs;
  return 0;
}

void wordfree (wordexp_t *we)
{
  size_t i;
  if (!we->we_wordv) return;
  for (i = 0; i < we->we_wordc; i++) free (we->we_wordv[we->we_offs + i]);
  free (we->we_wordv);
  we->we_wordv = 0; we->we_wordc = 0;
}
