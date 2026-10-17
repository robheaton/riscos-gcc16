/* glob.c - glob and globfree: the file names that match a pattern with * ? [..] in any directory component.  Directories are read with opendir/readdir, so names are found only where those work (with
   -munixlib; the SDK alone has stubs that fail).  Supported: GLOB_APPEND, GLOB_DOOFFS, GLOB_ERR, GLOB_MARK, GLOB_NOCHECK, GLOB_NOSORT, GLOB_NOMAGIC, GLOB_QUOTE (backslash), GLOB_BRACE ({a,b}).
   Not supported: GLOB_TILDE and GLOB_ALTDIRFUNC (ignored).  Names that begin with a dot match only a pattern that begins with a dot.  The SDK of RISC OS 64. */
#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <glob.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

struct list { char **v; int n, cap; };

static int add (struct list *l, const char *s)
{
  char *c;
  if (l->n + 2 > l->cap)
    {
      int cap = l->cap ? l->cap * 2 : 16;
      char **v = (char **) realloc (l->v, cap * sizeof *v);
      if (!v) return -1;
      l->v = v; l->cap = cap;
    }
  c = strdup (s);
  if (!c) return -1;
  l->v[l->n++] = c;
  l->v[l->n] = 0;
  return 0;
}

static int has_magic (const char *p, int quote)
{
  for (; *p; p++)
    {
      if (quote && *p == '\\' && p[1]) { p++; continue; }
      if (*p == '*' || *p == '?' || *p == '[') return 1;
    }
  return 0;
}

static void unquote (char *dst, const char *src, int quote)
{
  for (; *src; src++)
    {
      if (quote && *src == '\\' && src[1]) src++;
      *dst++ = *src;
    }
  *dst = 0;
}

/* prefix = the path matched so far ("" or ending in /), rest = the components still to match */
static int walk (const char *prefix, const char *rest, int flags, int (*errfunc) (const char *, int), struct list *out)
{
  char comp[1024], path[2048];
  const char *slash;
  size_t n;
  int quote = (flags & GLOB_QUOTE) != 0, last, r;
  while (*rest == '/') { rest++; if (!*prefix) { /* a rooted pattern */ if (strlen (prefix) + 2 < sizeof path) { strcpy (path, "/"); return walk (path, rest, flags, errfunc, out); } } }
  if (!*rest) return add (out, prefix);
  slash = strchr (rest, '/');
  n = slash ? (size_t) (slash - rest) : strlen (rest);
  if (n >= sizeof comp) return 0;
  memcpy (comp, rest, n); comp[n] = 0;
  last = !slash || !slash[1];
  rest = slash ? slash + 1 : rest + n;
  if (!has_magic (comp, quote))
    {
      char name[1024];
      struct stat st;
      unquote (name, comp, quote);
      if (strlen (prefix) + strlen (name) + 2 >= sizeof path) return 0;
      strcpy (path, prefix); strcat (path, name);
      if (stat (path, &st) != 0) return 0;			/* a name that is not there matches nothing */
      if (!last || slash) { if (!S_ISDIR (st.st_mode)) return 0; strcat (path, "/"); return walk (path, rest, flags, errfunc, out); }
      return add (out, path);
    }
  {
    DIR *d;
    struct dirent *e;
    d = opendir (*prefix ? prefix : ".");
    if (!d)
      {
	if ((errfunc && errfunc (prefix, errno)) || (flags & GLOB_ERR)) return GLOB_ABEND;
	return 0;
      }
    while ((e = readdir (d)) != 0)
      {
	struct stat st;
	if (fnmatch (comp, e->d_name, FNM_PERIOD | (quote ? 0 : FNM_NOESCAPE)) != 0) continue;
	if (strlen (prefix) + strlen (e->d_name) + 2 >= sizeof path) continue;
	strcpy (path, prefix); strcat (path, e->d_name);
	if (slash)
	  {
	    if (stat (path, &st) != 0 || !S_ISDIR (st.st_mode)) continue;
	    strcat (path, "/");
	    r = walk (path, rest, flags, errfunc, out);
	    if (r) { closedir (d); return r; }
	  }
	else if (add (out, path)) { closedir (d); return GLOB_NOSPACE; }
      }
    closedir (d);
  }
  return 0;
}

static int cmp (const void *a, const void *b) { return strcmp (*(char *const *) a, *(char *const *) b); }

static int glob1 (const char *pattern, int flags, int (*errfunc) (const char *, int), struct list *out)
{
  int r = walk ("", pattern, flags, errfunc, out);
  return r;
}

/* braces: {a,b} expands to several patterns; the result of each is added in turn */
static int glob_brace (const char *pat, int flags, int (*errfunc) (const char *, int), struct list *out)
{
  const char *o = strchr (pat, '{'), *p;
  int depth = 0;
  if (!(flags & GLOB_BRACE) || !o) return glob1 (pat, flags, errfunc, out);
  for (p = o; *p; p++)
    {
      if (*p == '{') depth++;
      else if (*p == '}' && --depth == 0) break;
    }
  if (!*p) return glob1 (pat, flags, errfunc, out);		/* no matching close: a plain pattern */
  {
    size_t pre = (size_t) (o - pat);
    const char *start = o + 1, *q = start;
    int d = 0, r = 0;
    char *buf = (char *) malloc (strlen (pat) + 1);
    if (!buf) return GLOB_NOSPACE;
    for (;; q++)
      {
	if (*q == '{') d++;
	else if ((*q == '}' && d-- == 0) || (*q == ',' && d == 0))
	  {
	    size_t alt = (size_t) (q - start);
	    memcpy (buf, pat, pre); memcpy (buf + pre, start, alt); strcpy (buf + pre + alt, p + 1);
	    r = glob_brace (buf, flags, errfunc, out);
	    if (r) break;
	    start = q + 1;
	    if (*q == '}') break;
	  }
	if (!*q) break;
      }
    free (buf);
    return r;
  }
}

int glob (const char *pattern, int flags, int (*errfunc) (const char *, int), glob_t *g)
{
  struct list l = { 0, 0, 0 };
  int r, i, base = 0, offs = (flags & GLOB_DOOFFS) ? g->gl_offs : 0;
  if (!(flags & GLOB_APPEND)) { g->gl_pathc = 0; g->gl_pathv = 0; g->gl_matchc = 0; if (!(flags & GLOB_DOOFFS)) g->gl_offs = 0; }
  g->gl_flags = flags;
  r = glob_brace (pattern, flags, errfunc, &l);
  if (r) { for (i = 0; i < l.n; i++) free (l.v[i]); free (l.v); return r; }
  if (l.n == 0)
    {
      if ((flags & GLOB_NOCHECK) || ((flags & GLOB_NOMAGIC) && !has_magic (pattern, (flags & GLOB_QUOTE) != 0)))
	{ char *c = (char *) malloc (strlen (pattern) + 1); if (!c) return GLOB_NOSPACE; unquote (c, pattern, (flags & GLOB_QUOTE) != 0); add (&l, c); free (c); }
      else { free (l.v); return GLOB_NOMATCH; }
    }
  else if (!(flags & GLOB_NOSORT)) qsort (l.v, l.n, sizeof *l.v, cmp);
  if (flags & GLOB_MARK)
    for (i = 0; i < l.n; i++)
      {
	struct stat st;
	size_t n = strlen (l.v[i]);
	if (n && l.v[i][n - 1] != '/' && stat (l.v[i], &st) == 0 && S_ISDIR (st.st_mode))
	  { char *c = (char *) realloc (l.v[i], n + 2); if (c) { c[n] = '/'; c[n + 1] = 0; l.v[i] = c; } }
      }
  {
    int old = (flags & GLOB_APPEND) && g->gl_pathv ? g->gl_pathc : 0;
    char **v = (char **) realloc (g->gl_pathv, (offs + old + l.n + 1) * sizeof *v);
    if (!v) { for (i = 0; i < l.n; i++) free (l.v[i]); free (l.v); return GLOB_NOSPACE; }
    if (!(flags & GLOB_APPEND)) for (i = 0; i < offs; i++) v[i] = 0;
    base = (flags & GLOB_APPEND) ? offs + old : offs;
    for (i = 0; i < l.n; i++) v[base + i] = l.v[i];
    v[base + l.n] = 0;
    g->gl_pathv = v;
    g->gl_pathc = (flags & GLOB_APPEND) ? old + l.n : l.n;
    g->gl_matchc = l.n;
  }
  free (l.v);
  return 0;
}

void globfree (glob_t *g)
{
  int i, offs = (g->gl_flags & GLOB_DOOFFS) ? g->gl_offs : 0;
  if (g->gl_pathv)
    {
      for (i = 0; i < g->gl_pathc; i++) free (g->gl_pathv[offs + i]);
      free (g->gl_pathv);
    }
  g->gl_pathv = 0; g->gl_pathc = 0;
}
