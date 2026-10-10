/* mkoslib - the C veneers of OSLib functions for a modkit module, made from OSLib's own headers: the C version of bin/mkoslib.py (the same output, byte for byte).

     mkoslib [-I OSLIB_INCLUDE_DIR] -o OUT.c [FUNCTION ...] [--from-objects A.o [B.o ...]]

There is no OSLib for the EABI (the libOSLib32.a of GCCSDK is built for the old ABI), but OSLib's C headers document the register assignment of every function in the comment above its declaration
(Input: name - value of R1 on entry; Output: name - value of R0 on exit (X version only); Other notes: Calls SWI 0x41200 [with R0 = 0x4 | with R0 |= 0x40]).  From that comment and the declaration this
makes the X function (xsocket_creat, xos_cli, ...): the inputs are put in their registers, the SWI is called through __modlib_xswi (modkit/lib/modswi.S: OS_CallASWI with the X bit), the outputs are stored
through the output pointers when there was no error, the result is the error block or NULL.  A pattern of the comment that it does not know is an error.  The non-X functions (socket_creat) are not made.

  --from-objects  makes the veneers of every OSLib X function that the object files use (their undefined symbols that the OSLib headers declare): the replacement for -lOSLib32 of GCCSDK 4.7.4.
                  A static library (.a) can be named too; what its members use is made as far as it can be (a function that cannot be made is a warning: the link shows whether it was needed)
  -I              the oslib folder of OSLib's headers; default: $OSLIB/oslib when the environment variable OSLIB is set
The header of a function is found by its name (xosfile_delete is in osfile.h, xwimp_start_task in wimp.h); the other headers of the folder are searched when it is not there. */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <setjmp.h>
#if !defined (__riscos__)
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif
#include "modcommon.h"

static int is_word (int c) { return isalnum (c) || c == '_'; }
static int is_space (int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }

/* ---------------------------------------------------------------- finding the declaration */
typedef struct { char *header; char *text; size_t pos; } Decl;      /* header: the name for #include "oslib/<header>" */

/* the offset of "extern os_error *NAME (" at the start of a line in TEXT, or (size_t) -1 */
static size_t find_in_text (const char *text, const char *name)
{
  const char *p = text;
  size_t nl = strlen (name);
  static const char pre[] = "extern os_error *";
  for (;;)
    {
      if (strncmp (p, pre, sizeof pre - 1) == 0 && strncmp (p + sizeof pre - 1, name, nl) == 0 && p[sizeof pre - 1 + nl] == ' ' && p[sizeof pre + nl] == '(') return (size_t) (p - text);
      p = strchr (p, '\n');
      if (!p) return (size_t) -1;
      p++;
    }
}

static char *header_path (const char *dir, const char *leaf)
{
  Buf b;
  buf_init (&b);
  buf_printf (&b, "%s/%s", dir, leaf);
  return b.s;
}

/* a header is read once: a module can use some hundreds of OSLib functions, and the headers are big (wimp.h is 700 KB) */
typedef struct { char *dir, *hdr; unsigned char *text; } Cached;
static Cached *cache;
static int ncache;

static unsigned char *cached_text (const char *dir, const char *hdr)
{
  int i;
  unsigned char *text = NULL;
  for (i = 0; i < ncache; i++)
    if (strcmp (cache[i].dir, dir) == 0 && strcmp (cache[i].hdr, hdr) == 0) { text = cache[i].text; break; }
  if (i == ncache)
    {
      char *path = header_path (dir, hdr);
      FILE *f = fopen (path, "rb");
      if (f) { fclose (f); text = read_file (path, NULL); }
      free (path);
      cache = xrealloc (cache, sizeof (Cached) * (size_t) (ncache + 1));
      cache[ncache].dir = xstrdup (dir);
      cache[ncache].hdr = xstrdup (hdr);
      cache[ncache].text = text;                              /* NULL: there is no such header (remembered too) */
      ncache++;
    }
  return text;
}

static int try_header (const char *dir, const char *hdr, const char *name, Decl *d)
{
  size_t pos;
  unsigned char *text = cached_text (dir, hdr);
  if (!text) return 0;
  pos = find_in_text ((const char *) text, name);
  if (pos == (size_t) -1) return 0;
  d->header = xstrdup (hdr);
  d->text = (char *) text;
  d->pos = pos;
  return 1;
}

static int cmp_str (const void *a, const void *b) { return strcmp (*(char *const *) a, *(char *const *) b); }

#if !defined (__riscos__)
static int scan_all (const char *dir, const char *name, Decl *d)
{
  DIR *dp = opendir (dir);
  struct dirent *e;
  char **names = NULL;
  int n = 0, i;
  if (!dp) return 0;
  while ((e = readdir (dp)) != NULL)
    {
      size_t l = strlen (e->d_name);
      if (l > 2 && strcmp (e->d_name + l - 2, ".h") == 0)
        {
          names = xrealloc (names, sizeof (char *) * (size_t) (n + 1));
          names[n++] = xstrdup (e->d_name);
        }
    }
  closedir (dp);
  qsort (names, (size_t) n, sizeof (char *), cmp_str);
  for (i = 0; i < n; i++)
    if (try_header (dir, names[i], name, d)) return 1;
  return 0;
}
#endif

static Decl find_decl (const char *dir, const char *name)
{
  Decl d;
  const char *us = strchr (name, '_');
  char *hdr;
  /* by name: xosfile_delete -> osfile.h */
  if (name[0] == 'x' && us && us > name + 1)
    {
      hdr = xmalloc ((size_t) (us - name) + 4);
      memcpy (hdr, name + 1, (size_t) (us - name - 1));
      strcpy (hdr + (us - name - 1), ".h");
      if (try_header (dir, hdr, name, &d)) { free (hdr); return d; }
      free (hdr);
    }
#if !defined (__riscos__)
  if (scan_all (dir, name, &d)) return d;
#endif
  die ("%s is not declared in the OSLib headers of %s", name, dir);
}

/* ---------------------------------------------------------------- parsing the declaration and its comment */
typedef struct { char *type; char *name; char *text; } Param;
typedef struct { char *name; int reg; } Reg;              /* a parameter and the register that the comment gives it */

typedef struct
{
  char *name, *header;
  Param *params; int nparams;
  Reg *in; int nin;
  Reg *out; int nout;
  unsigned swi;
  char *comp[8]; int comp_k[8]; int ncomp;                         /* "w - component 0": parameters that go into a block of words */
  int has_block; int block_reg; char *block_name[8]; int nblock;
  int nops; int op_reg[8]; int op_kind[8]; unsigned op_val[8];     /* "with R1 |= 0x3, R3 = 0x0": kind 0 =, 1 |=, 2 += */
  int ret_reg;                                                     /* "Returns: R3 (non-X version only)": 3, "psr": 16, none: -1 */
  const char *dtext; size_t dend;                                  /* the text of the header and the offset of the ';' that ends the X declaration */
  int nonx; char *rtype;                                           /* a non-X function: the type of its result */
} Func;

static char *normalise (const char *s, size_t n)             /* Python: re.sub (r"\s+", " ", s) */
{
  Buf b;
  size_t i;
  int sp = 0;
  buf_init (&b);
  for (i = 0; i < n; i++)
    {
      if (is_space ((unsigned char) s[i])) { sp = 1; continue; }
      if (sp) { buf_addc (&b, ' '); sp = 0; }
      buf_addc (&b, s[i]);
    }
  if (sp) buf_addc (&b, ' ');
  return b.s;
}

static char *strip_ws (const char *s, size_t n)
{
  size_t a = 0;
  while (a < n && is_space ((unsigned char) s[a])) a++;
  while (n > a && is_space ((unsigned char) s[n - 1])) n--;
  return xstrndup (s + a, n - a);
}

static void set_reg (Reg **arr, int *n, const char *name, int reg)       /* a dict: a name that is there already keeps its place and gets the new register */
{
  int i;
  for (i = 0; i < *n; i++)
    if (strcmp ((*arr)[i].name, name) == 0) { (*arr)[i].reg = reg; return; }
  *arr = xrealloc (*arr, sizeof (Reg) * (size_t) (*n + 1));
  (*arr)[*n].name = xstrdup (name);
  (*arr)[*n].reg = reg;
  (*n)++;
}

static int find_reg (const Reg *arr, int n, const char *name)
{
  int i;
  for (i = 0; i < n; i++) if (strcmp (arr[i].name, name) == 0) return 1;
  return 0;
}

/* LINE matches  [PREFIX\s*](\w+) - value of R(\d+) on <TAIL>  ... \s*$  with TAIL "entry" or "exit" (+ the optional " (X version only)" for exit) */
static int match_reg_line (const char *line, const char *prefix, const char *tail, int optional_x, char **name, int *reg)
{
  const char *p = line, *q;
  size_t pl = strlen (prefix), n;
  if (strncmp (p, prefix, pl) == 0) { p += pl; while (is_space ((unsigned char) *p)) p++; }
  q = p;
  while (is_word ((unsigned char) *q)) q++;
  if (q == p) return 0;
  n = (size_t) (q - p);
  if (strncmp (q, " - value of R", 13) != 0) return 0;
  q += 13;
  if (!isdigit ((unsigned char) *q)) return 0;
  *reg = 0;
  while (isdigit ((unsigned char) *q)) *reg = *reg * 10 + (*q++ - '0');
  if (strncmp (q, " on ", 4) != 0) return 0;
  q += 4;
  if (strncmp (q, tail, strlen (tail)) != 0) return 0;
  q += strlen (tail);
  if (optional_x && strncmp (q, " (X version only)", 17) == 0) q += 17;
  while (is_space ((unsigned char) *q)) q++;
  if (*q) return 0;
  *name = xstrndup (p, n);
  return 1;
}

/* LINE matches  [Output:\s*](\w+) - processor status register on exit( \(X version only\))?\s*$ : the flags of the SWI */
static int match_psr_line (const char *line, char **name)
{
  const char *p = line, *q;
  static const char pre[] = "Output:", mid[] = " - processor status register on exit";
  if (strncmp (p, pre, sizeof pre - 1) == 0) { p += sizeof pre - 1; while (is_space ((unsigned char) *p)) p++; }
  q = p;
  while (is_word ((unsigned char) *q)) q++;
  if (q == p || strncmp (q, mid, sizeof mid - 1) != 0) return 0;
  *name = xstrndup (p, (size_t) (q - p));
  q += sizeof mid - 1;
  if (strncmp (q, " (X version only)", 17) == 0) q += 17;
  while (is_space ((unsigned char) *q)) q++;
  if (*q) { free (*name); return 0; }
  return 1;
}

static int has_value_of_r (const char *line)               /* re.search (r"value of R\d+ on (entry|exit)", line) */
{
  const char *p = line;
  while ((p = strstr (p, "value of R")) != NULL)
    {
      const char *q = p + 10;
      if (isdigit ((unsigned char) *q))
        {
          while (isdigit ((unsigned char) *q)) q++;
          if (strncmp (q, " on entry", 9) == 0 || strncmp (q, " on exit", 8) == 0) return 1;
        }
      p++;
    }
  return 0;
}

static int has_r_equals (const char *line)                 /* re.search (r"\bR\d+ (= |\|= )", part of the line before "Calls SWI"); 0 when there is no "Calls SWI" */
{
  const char *cs = strstr (line, "Calls SWI"), *p;
  if (!cs) return 0;
  for (p = line; p < cs; p++)
    {
      const char *q;
      if (*p != 'R' || (p > line && is_word ((unsigned char) p[-1]))) continue;
      q = p + 1;
      if (!isdigit ((unsigned char) *q)) continue;
      while (isdigit ((unsigned char) *q) && q < cs) q++;
      if (q >= cs || *q != ' ') continue;
      q++;
      if (q + 2 <= cs && strncmp (q, "= ", 2) == 0) return 1;
      if (q + 3 <= cs && strncmp (q, "|= ", 3) == 0) return 1;
    }
  return 0;
}

static unsigned parse_hex_at (const char *p, const char **end)
{
  unsigned v = 0;
  p += 2;
  while (isxdigit ((unsigned char) *p)) { v = v * 16 + (unsigned) (isdigit ((unsigned char) *p) ? *p - '0' : tolower ((unsigned char) *p) - 'a' + 10); p++; }
  *end = p;
  return v;
}

/* LINE matches  [Input:\s*](\w+) - component (\d+)\s*$ */
static int match_component (const char *line, char **name, int *k)
{
  const char *p = line, *q;
  if (strncmp (p, "Input:", 6) == 0) { p += 6; while (is_space ((unsigned char) *p)) p++; }
  q = p;
  while (is_word ((unsigned char) *q)) q++;
  if (q == p || strncmp (q, " - component ", 13) != 0) return 0;
  *name = xstrndup (p, (size_t) (q - p));
  q += 13;
  if (!isdigit ((unsigned char) *q)) { free (*name); return 0; }
  *k = 0;
  while (isdigit ((unsigned char) *q)) { if (*k < 100000) *k = *k * 10 + (*q - '0'); q++; }
  while (is_space ((unsigned char) *q)) q++;
  if (*q) { free (*name); return 0; }
  return 1;
}

/* OSLib's own register table of a SWI (the  <Module>.Hdr  file next to the header):  Entry / Exit lines  "R1 -> s (String)",  "R2 = station (Int)";  Entry lines that are blocks of components
   "R1 -> delete_icon (sequence of (Wimp_W, Wimp_I))".  Only on hosts that can list a folder (not RISC OS: there the parameters keep the comment's registers) */
typedef struct { Reg *e; int ne; Reg *x; int nx; int blockreg[4]; int nblocks; } HdrInfo;

#if !defined (__riscos__)
static HdrInfo hdr_entries (const char *dir, const char *header, unsigned swi)
{
  HdrInfo h;
  char base[256], *txt = NULL, *ln, *nx;
  size_t bl = 0;
  DIR *dp;
  struct dirent *de;
  char path[2048];
  int found = 0, section = 0;
  memset (&h, 0, sizeof h);
  while (header[bl] && header[bl] != '.' && bl < sizeof base - 1) { base[bl] = (char) tolower ((unsigned char) header[bl]); bl++; }
  base[bl] = 0;
  dp = opendir (dir);
  if (!dp) return h;
  path[0] = 0;
  while ((de = readdir (dp)) != NULL)
    {
      size_t nl = strlen (de->d_name);
      char lower[256];
      size_t i;
      if (nl != bl + 4 || nl >= sizeof lower) continue;
      for (i = 0; i < nl; i++) lower[i] = (char) tolower ((unsigned char) de->d_name[i]);
      lower[nl] = 0;
      if (strncmp (lower, base, bl) == 0 && strcmp (lower + bl, ".hdr") == 0) { snprintf (path, sizeof path, "%s/%s", dir, de->d_name); break; }
    }
  closedir (dp);
  if (!path[0])                                          /* the layout of the RISC OS sources: Hdr/Wimp (objasm syntax) in the folder of the headers */
    {
      char sub[1024];
      snprintf (sub, sizeof sub, "%s/Hdr", dir);
      dp = opendir (sub);
      if (dp)
        {
          while ((de = readdir (dp)) != NULL)
            {
              size_t nl = strlen (de->d_name), i;
              char lower[256];
              if (nl != bl || nl >= sizeof lower) continue;
              for (i = 0; i < nl; i++) lower[i] = (char) tolower ((unsigned char) de->d_name[i]);
              lower[nl] = 0;
              if (strcmp (lower, base) == 0) { snprintf (path, sizeof path, "%s/%s", sub, de->d_name); break; }
            }
          closedir (dp);
        }
    }
  if (!path[0]) return h;
  {
    FILE *fp = fopen (path, "rb");
    long n;
    if (!fp) return h;
    fseek (fp, 0, SEEK_END); n = ftell (fp); fseek (fp, 0, SEEK_SET);
    txt = xmalloc ((size_t) n + 1);
    if (fread (txt, 1, (size_t) n, fp) != (size_t) n) n = 0;
    txt[n] = 0;
    fclose (fp);
  }
  for (ln = txt; ln && *ln; ln = nx)
    {
      char *e;
      nx = strchr (ln, '\n');
      if (nx) { *nx = 0; nx++; }
      e = ln + strlen (ln);
      while (e > ln && (e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t')) *--e = 0;
      if (!found)
        {
          const char *q = ln;
          unsigned v = 0;
          int ok = 0;
          if (strncmp (q, ".set ", 5) == 0)                  /* gas:  .set Name,0x4000f */
            {
              q += 5;
              if (*q == 'X') q++;
              if (!is_word ((unsigned char) *q)) continue;
              while (is_word ((unsigned char) *q)) q++;
              if (strncmp (q, ",0x", 3) != 0 || !isxdigit ((unsigned char) q[3])) continue;
              q += 3;
              ok = 1;
            }
          else                                                /* objasm:  Name   *   &4000F */
            {
              if (!is_word ((unsigned char) *q)) continue;
              while (is_word ((unsigned char) *q)) q++;
              while (*q == ' ' || *q == '\t') q++;
              if (*q != '*') continue;
              q++;
              while (*q == ' ' || *q == '\t') q++;
              if (*q != '&' || !isxdigit ((unsigned char) q[1])) continue;
              q++;
              ok = 1;
            }
          if (!ok) continue;
          while (isxdigit ((unsigned char) *q)) { v = v * 16 + (unsigned) (isdigit ((unsigned char) *q) ? *q - '0' : tolower ((unsigned char) *q) - 'a' + 10); q++; }
          if (*q == 0 && v == swi && nx && (nx[0] == ' ' || nx[0] == '\t') && (strchr (nx, '@') != NULL || strchr (nx, ';') != NULL))
            {
              const char *w = nx;                               /* the next line must be a comment line:  white space, then @ or ;  */
              while (*w == ' ' || *w == '\t') w++;
              if (*w == '@' || *w == ';') found = 1;
            }
          continue;
        }
      {
        const char *t = ln;
        while (*t == ' ' || *t == '\t') t++;
        if (t == ln || (*t != '@' && *t != ';')) break;
        t++;
        char name[128];
        int reg = 0, ptr;
        size_t k;
        while (*t == ' ' || *t == '\t') t++;
        if (strcmp (t, "Entry") == 0) { section = 1; continue; }
        if (strcmp (t, "Exit") == 0) { section = 2; continue; }
        if (t[0] != 'R' || !isdigit ((unsigned char) t[1]) || !section) continue;
        t++;
        while (isdigit ((unsigned char) *t)) { reg = reg * 10 + (*t - '0'); t++; }
        if (strncmp (t, " = ", 3) == 0) { ptr = 0; t += 3; }
        else if (strncmp (t, " -> ", 4) == 0) { ptr = 1; t += 4; }
        else continue;
        k = 0;
        while (is_word ((unsigned char) *t) && k < sizeof name - 1) name[k++] = *t++;
        name[k] = 0;
        if (!k) continue;
        if (*t == ' ' && t[1] == '(') { if (t[strlen (t) - 1] != ')') continue; }
        else if (*t) continue;
        {
          Reg **arr = section == 1 ? &h.e : &h.x;
          int *cnt = section == 1 ? &h.ne : &h.nx, i, dup = 0;
          for (i = 0; i < *cnt; i++) if (strcmp ((*arr)[i].name, name) == 0) dup = 1;
          if (!dup) { *arr = xrealloc (*arr, sizeof (Reg) * (size_t) (*cnt + 1)); (*arr)[*cnt].name = xstrdup (name); (*arr)[*cnt].reg = reg; (*cnt)++; }
        }
        if (section == 1 && ptr && strncmp (t, " (sequence of", 13) == 0 && h.nblocks < 4) h.blockreg[h.nblocks++] = reg;
      }
    }
  free (txt);
  return h;
}
#endif

/* LINE (after the leading spaces and stars) is  Returns:\s+(R(\d+)|psr) \(non-X version only\)\s*  : *REG is the register (16 for psr) */
static int match_returns (const char *line, int *reg)
{
  const char *p = line;
  static const char pre[] = "Returns:", tail[] = " (non-X version only)";
  int r = 0;
  if (strncmp (p, pre, sizeof pre - 1) != 0) return 0;
  p += sizeof pre - 1;
  if (!is_space ((unsigned char) *p)) return 0;
  while (is_space ((unsigned char) *p)) p++;
  if (strncmp (p, "psr", 3) == 0) { r = 16; p += 3; }
  else if (p[0] == 'R' && isdigit ((unsigned char) p[1]))
    {
      p++;
      while (isdigit ((unsigned char) *p)) r = r * 10 + (*p++ - '0');
    }
  else return 0;
  if (strncmp (p, tail, sizeof tail - 1) != 0) return 0;
  p += sizeof tail - 1;
  while (is_space ((unsigned char) *p)) p++;
  if (*p) return 0;
  *reg = r;
  return 1;
}

static void parse_plist (Func *f, const char *name, const char *plist)
{
  if (strcmp (plist, "void") != 0)
    {
      Buf cur;
      int depth = 0;
      size_t j;
      buf_init (&cur);
      for (j = 0; j <= strlen (plist); j++)
        {
          char ch = plist[j];
          int endp = ch == 0;
          if (!endp && (ch == '(' || ch == '[')) depth++;
          if (!endp && (ch == ')' || ch == ']')) depth--;
          if (endp || (ch == ',' && depth == 0))
            {
              char *p = strip_ws (cur.s, cur.len);
              if (endp && !*p) { free (p); break; }                 /* Python: a last part that is empty is dropped (an empty one in the middle is not) */
              {
                size_t pl = strlen (p), nstart = pl;
                char *ty;
                while (nstart > 0 && is_word ((unsigned char) p[nstart - 1])) nstart--;
                if (nstart == pl) die ("%s: cannot read the parameter '%s'", name, p);
                ty = strip_ws (p, nstart);
                f->params = xrealloc (f->params, sizeof (Param) * (size_t) (f->nparams + 1));
                f->params[f->nparams].type = ty;
                f->params[f->nparams].name = xstrdup (p + nstart);
                f->params[f->nparams].text = p;
                f->nparams++;
              }
              cur.len = 0; cur.s[0] = 0;
              if (endp) break;
            }
          else buf_addc (&cur, ch);
        }
    }
}

static int is_nonx (const char *dir, const char *name);
static Func parse_nonx (const char *dir, const char *name);

static Func parse_func (const char *dir, const char *name)
{
  if (is_nonx (dir, name)) return parse_nonx (dir, name);
  Decl d = find_decl (dir, name);
  Func f;
  const char *txt = d.text, *cstart = NULL, *cend, *dend, *s, *par, *close;
  char *comment, *decl, *norm, *plist, *line, *savep;
  static const char marker[] = "/* ------";
  size_t csize, k;
  int i;

  memset (&f, 0, sizeof f);
  f.name = xstrdup (name);
  f.header = d.header;
  f.ret_reg = -1;
  f.dtext = txt;
  /* the comment above the declaration: from the last comment opening of the form slash-star-space-dashes before it to the next end of comment */
  if (d.pos >= sizeof marker - 1)
    {
      size_t idx;
      for (idx = d.pos - (sizeof marker - 1) + 1; idx-- > 0;)
        if (strncmp (txt + idx, marker, sizeof marker - 1) == 0) { cstart = txt + idx; break; }
    }
  (void) s;
  if (!cstart) die ("%s: no comment above its declaration", name);
  cend = strstr (cstart, "*/");
  if (!cend) die ("%s: the comment above its declaration is not closed", name);
  csize = (size_t) (cend - cstart);
  comment = xstrndup (cstart, csize);
  dend = strchr (txt + d.pos, ';');
  if (!dend) die ("%s: the declaration has no ;", name);
  f.dend = (size_t) (dend - txt);
  decl = xstrndup (txt + d.pos, (size_t) (dend - (txt + d.pos)));
  norm = normalise (decl, strlen (decl));
  /* extern os_error *NAME (PARAMS) */
  {
    static const char pre[] = "extern os_error *";
    size_t nl = strlen (name);
    k = strlen (norm);
    while (k && is_space ((unsigned char) norm[k - 1])) k--;
    if (strncmp (norm, pre, sizeof pre - 1) != 0 || strncmp (norm + sizeof pre - 1, name, nl) != 0 || strncmp (norm + sizeof pre - 1 + nl, " (", 2) != 0 || k == 0 || norm[k - 1] != ')')
      die ("%s: cannot read its declaration '%s'", name, norm);
    par = norm + sizeof pre - 1 + nl + 2;
    close = norm + k - 1;
    plist = strip_ws (par, (size_t) (close - par));
  }
  parse_plist (&f, name, plist);
  /* the comment */
  savep = comment;
  for (;;)
    {
      char *nl = strchr (savep, '\n');
      char *l, *nm = NULL;
      int reg;
      size_t ll = nl ? (size_t) (nl - savep) : strlen (savep);
      line = xstrndup (savep, ll);
      l = line;
      while (*l == ' ' || *l == '*') l++;
      if (f.ret_reg < 0) match_returns (l, &f.ret_reg);
      if (match_reg_line (l, "Input:", "entry", 0, &nm, &reg)) set_reg (&f.in, &f.nin, nm, reg);
      else if (match_reg_line (l, "Output:", "exit", 1, &nm, &reg)) set_reg (&f.out, &f.nout, nm, reg);
      else if (match_component (l, &nm, &reg)) { if (f.ncomp < 8) { f.comp[f.ncomp] = xstrdup (nm); f.comp_k[f.ncomp++] = reg; } }
      else if (match_psr_line (l, &nm)) set_reg (&f.out, &f.nout, nm, 16);                   /* the flags of the SWI: register 16 */
      else if (has_value_of_r (l) || has_r_equals (l)) die ("%s: the comment line '%s' is not a pattern that mkoslib knows", name, l);
      free (nm); free (line);
      if (!nl) break;
      savep = nl + 1;
    }
  {
    const char *p = comment, *e;
    for (;;)
      {
        p = strstr (p, "Calls SWI 0x");
        if (!p) die ("%s: no 'Calls SWI' in its comment", name);
        if (isxdigit ((unsigned char) p[12])) break;
        p++;
      }
    f.swi = parse_hex_at (p + 10, &e);
    if (strncmp (e, " with ", 6) == 0)                       /* constants put into registers after the inputs (up to 16.2.0-17 only R0 was understood and the others were dropped) */
      {
        const char *q = e + 6;
        for (;;)
          {
            int kind;
            const char *item = q;
            if (q[0] != 'R' || !isdigit ((unsigned char) q[1])) die ("%s: the comment part '%.30s' is not a pattern that mkoslib knows", name, item);
            if (f.nops >= 8) die ("%s: too many register settings in the comment", name);
            f.op_reg[f.nops] = q[1] - '0';
            q += 2;
            if (strncmp (q, " = ", 3) == 0) { kind = 0; q += 3; }
            else if (strncmp (q, " |= ", 4) == 0) { kind = 1; q += 4; }
            else if (strncmp (q, " += ", 4) == 0) { kind = 2; q += 4; }
            else die ("%s: the comment part '%.30s' is not a pattern that mkoslib knows", name, item);
            if (!(q[0] == '0' && q[1] == 'x' && isxdigit ((unsigned char) q[2]))) die ("%s: the comment part '%.30s' is not a pattern that mkoslib knows", name, item);
            f.op_kind[f.nops] = kind;
            f.op_val[f.nops] = parse_hex_at (q, &q);
            f.nops++;
            if (q[0] == ',' && q[1] == ' ') { q += 2; continue; }
            if (q[0] == '.' && (q[1] == 0 || q[1] == '\n' || q[1] == '\r' || q[1] == ' ')) break;
            if (q[0] == 0 || q[0] == '\n' || q[0] == '\r') break;
            die ("%s: the comment part '%.30s' is not a pattern that mkoslib knows", name, q);
          }
      }
  }
#if !defined (__riscos__)
  {                                                       /* the parameters that the comment gives no register: OSLib's Hdr file has the table of the SWI */
    int missing = 0, j;
    for (j = 0; j < f.nparams; j++)
      {
        int iscomp = 0, c;
        for (c = 0; c < f.ncomp; c++) if (strcmp (f.comp[c], f.params[j].name) == 0) iscomp = 1;
        if (!iscomp && !find_reg (f.in, f.nin, f.params[j].name) && !find_reg (f.out, f.nout, f.params[j].name)) missing = 1;
      }
    if (missing || f.ncomp)
      {
        HdrInfo h = hdr_entries (dir, d.header, f.swi);
        for (j = 0; j < f.nparams; j++)
          {
            int iscomp = 0, c, k;
            for (c = 0; c < f.ncomp; c++) if (strcmp (f.comp[c], f.params[j].name) == 0) iscomp = 1;
            if (iscomp || find_reg (f.in, f.nin, f.params[j].name) || find_reg (f.out, f.nout, f.params[j].name)) continue;
            for (k = 0; k < h.ne; k++) if (strcmp (h.e[k].name, f.params[j].name) == 0) { set_reg (&f.in, &f.nin, f.params[j].name, h.e[k].reg); goto found; }
            for (k = 0; k < h.nx; k++) if (strcmp (h.x[k].name, f.params[j].name) == 0) { set_reg (&f.out, &f.nout, f.params[j].name, h.x[k].reg); goto found; }
          found: ;
          }
        if (f.ncomp)
          {
            int a, b;
            if (h.nblocks != 1) die ("%s: the parameters are components of a block, but the Hdr file of OSLib does not show one block (%d)", name, h.nblocks);
            f.has_block = 1; f.block_reg = h.blockreg[0];
            for (a = 0; a < f.ncomp; a++)                    /* by component number (stable) */
              {
                int best = -1;
                for (b = 0; b < f.ncomp; b++)
                  {
                    int used = 0, u;
                    for (u = 0; u < f.nblock; u++) if (strcmp (f.block_name[u], f.comp[b]) == 0) used = 1;
                    if (!used && (best < 0 || f.comp_k[b] < f.comp_k[best])) best = b;
                  }
                f.block_name[f.nblock++] = f.comp[best];
              }
          }
      }
  }
#endif
  (void) i;
  free (comment); free (decl); free (norm); free (plist);
  return f;
}

/* the non-X function NAME (os_cli): the data of the X function (xos_cli), but the parameters and the result of the declaration of the non-X function that follows it; an output that is for the X version only has
   no parameter there, the result is the register that the "Returns:" line names, the type of the result is the one of the declaration; an error calls __modlib_raise */
static Func parse_nonx (const char *dir, const char *name)
{
  char *xname = xmalloc (strlen (name) + 2);
  Func f;
  const char *p, *q, *rest, *par, *semi;
  char *slice, *prefix, *plist, *rtype;
  size_t n, nl = strlen (name), k;
  int i, j;
  xname[0] = 'x'; strcpy (xname + 1, name);
  f = parse_func (dir, xname);
  free (xname);
  n = strlen (f.dtext + f.dend + 1);
  if (n > 2999) n = 2999;
  slice = xstrndup (f.dtext + f.dend + 1, n);
  p = slice;
  while (is_space ((unsigned char) *p)) p++;
  if (strncmp (p, "extern ", 7) == 0) p += 7;
  else if (strncmp (p, "__swi (0x", 9) == 0 && isxdigit ((unsigned char) p[9]))
    {
      q = p + 9;
      while (isxdigit ((unsigned char) *q)) q++;
      if (strncmp (q, ") ", 2) != 0) die ("%s: no declaration of the non-X function follows the X function", name);
      p = q + 2;
    }
  else die ("%s: no declaration of the non-X function follows the X function", name);
  /* [^;(]+? then an optional white space, the name at a word boundary, " (" */
  rest = NULL;
  for (q = p + 1; *q && q[-1] != ';' && q[-1] != '('; q++)
    if (strncmp (q, name, nl) == 0 && q[nl] == ' ' && q[nl + 1] == '(' && !is_word ((unsigned char) q[-1])) { rest = q; break; }
  if (!rest) die ("%s: no declaration of the non-X function follows the X function", name);
  prefix = normalise (p, (size_t) (rest - p));
  k = strlen (prefix);
  rtype = strip_ws (prefix, k);
  free (prefix);
  par = rest + nl + 2;
  semi = strchr (par, ';');
  if (!semi || semi == par || semi[-1] != ')') die ("%s: no declaration of the non-X function follows the X function", name);
  {
    char *raw = xstrndup (par, (size_t) (semi - 1 - par));
    char *norm = normalise (raw, strlen (raw));
    plist = strip_ws (norm, strlen (norm));
    free (raw); free (norm);
  }
  free (slice);
  {
    Func g = f;
    Reg *keep = NULL;
    int nkeep = 0;
    g.params = NULL; g.nparams = 0;
    parse_plist (&g, name, plist);
    for (i = 0; i < g.nparams; i++)
      {
        int found = 0;
        for (j = 0; j < f.nparams; j++) if (strcmp (f.params[j].name, g.params[i].name) == 0) found = 1;
        if (!found) die ("%s: the parameter '%s' is not one of the X function's", name, g.params[i].name);
      }
    if (strcmp (rtype, "void") == 0)
      {
        if (f.ret_reg >= 0) die ("%s: it returns void, and the comment says it returns a register", name);
      }
    else if (f.ret_reg < 0) die ("%s: it returns %s, and the comment has no 'Returns:' line", name, rtype);
    for (i = 0; i < f.nout; i++)                                    /* the outputs that are not parameters of the non-X function are for the X version only */
      for (j = 0; j < g.nparams; j++)
        if (strcmp (f.out[i].name, g.params[j].name) == 0)
          {
            keep = xrealloc (keep, sizeof (Reg) * (size_t) (nkeep + 1));
            keep[nkeep++] = f.out[i];
            break;
          }
    g.out = keep; g.nout = nkeep;
    g.name = xstrdup (name);
    g.nonx = 1; g.rtype = rtype;
    free (plist);
    return g;
  }
}

/* ---------------------------------------------------------------- the veneers */
static Param *find_param (const Func *f, const char *name)
{
  int i;
  for (i = 0; i < f->nparams; i++) if (strcmp (f->params[i].name, name) == 0) return &f->params[i];
  return NULL;
}

static void sort_regs (Reg *a, int n)                    /* by register, stable (Python's sorted () with a key) */
{
  int i, j;
  for (i = 1; i < n; i++)
    {
      Reg t = a[i];
      for (j = i - 1; j >= 0 && a[j].reg > t.reg; j--) a[j + 1] = a[j];
      a[j + 1] = t;
    }
}

static int in_block (const Func *f, const char *name)
{
  int i;
  for (i = 0; i < f->nblock; i++) if (strcmp (f->block_name[i], name) == 0) return 1;
  return 0;
}

static char *generate (const char *dir, char **funcs, int nfuncs)
{
  Buf out, body;
  char **heads = NULL;
  int nheads = 0, fi, i, j, flags, uses_flags = 0, uses_raise = 0;
  buf_init (&out); buf_init (&body);
  for (fi = 0; fi < nfuncs; fi++)
    {
      Func f = parse_func (dir, funcs[fi]);
      Buf L, outs;
      int seen = 0;
      flags = 0;
      for (i = 0; i < nheads; i++) if (strcmp (heads[i], f.header) == 0) seen = 1;
      if (!seen) { heads = xrealloc (heads, sizeof (char *) * (size_t) (nheads + 1)); heads[nheads++] = f.header; }
      buf_init (&L); buf_init (&outs);
      if (f.nonx) buf_printf (&L, "%s%s%s (", f.rtype, f.rtype[0] && f.rtype[strlen (f.rtype) - 1] == '*' ? "" : " ", f.name);
      else buf_printf (&L, "os_error *%s (", f.name);
      if (!f.nparams) buf_adds (&L, "void");
      for (i = 0; i < f.nparams; i++) { if (i) buf_adds (&L, ", "); buf_adds (&L, f.params[i].text); }
      for (i = 0; i < f.nout; i++) if (f.out[i].reg == 16) flags = 1;
      if (f.nonx && f.ret_reg == 16) flags = 1;
      uses_flags |= flags;
      uses_raise |= f.nonx;
      buf_adds (&L, ")\n{\n  unsigned _r[10] = { 0 };\n");
      if (flags) buf_adds (&L, "  unsigned _flags = 0;\n");
      sort_regs (f.in, f.nin);
      if (f.has_block)
        {
          for (i = 0; i < f.nblock; i++)
            {
              Param *bp = find_param (&f, f.block_name[i]);
              char *ty;
              if (!bp) die ("%s: the comment names a component '%s' that is not a parameter", f.name, f.block_name[i]);
              ty = strip_ws (bp->type, strlen (bp->type));
              buf_printf (&L, "  _Static_assert (sizeof (%s) == 4, \"%s is not a word\");\n", ty, f.block_name[i]);
              free (ty);
            }
          buf_printf (&L, "  unsigned _blk[%d] = { ", f.nblock);
          for (i = 0; i < f.nblock; i++) buf_printf (&L, "%s(unsigned) %s", i ? ", " : "", f.block_name[i]);
          buf_printf (&L, " };\n  _r[%d] = (unsigned) _blk;\n", f.block_reg);
        }
      for (i = 0; i < f.nin; i++)
        {
          if (!find_param (&f, f.in[i].name)) die ("%s: the comment names an input '%s' that is not a parameter", f.name, f.in[i].name);
          buf_printf (&L, "  _r[%d] = (unsigned) %s;\n", f.in[i].reg, f.in[i].name);
        }
      for (i = 0; i < f.nops; i++) buf_printf (&L, "  _r[%d] %s %s;\n", f.op_reg[i], f.op_kind[i] == 1 ? "|=" : f.op_kind[i] == 2 ? "+=" : "=", hx (f.op_val[i]));
      buf_printf (&L, "  os_error *_e = (os_error *) %s (%s, _r%s);\n", flags ? "__modlib_xswif" : "__modlib_xswi", hx (0x20000u | f.swi), flags ? ", &_flags" : "");
      if (f.nonx) buf_adds (&L, "  if (_e)\n    __modlib_raise (_e);\n");
      sort_regs (f.out, f.nout);
      for (i = 0; i < f.nout; i++)
        {
          Param *p = find_param (&f, f.out[i].name);
          char *ty, *pointee;
          size_t tl;
          if (!p) die ("%s: the comment names an output '%s' that is not a parameter", f.name, f.out[i].name);
          ty = strip_ws (p->type, strlen (p->type));
          tl = strlen (ty);
          if (!tl || ty[tl - 1] != '*') die ("%s: output '%s' is not a pointer parameter (%s)", f.name, f.out[i].name, ty);
          pointee = strip_ws (ty, tl - 1);
          if (outs.len == 0 && i == 0) {}
          if (i) buf_addc (&outs, '\n');
          if (f.out[i].reg == 16) buf_printf (&outs, "%sif (%s) *%s = (%s) _flags;", f.nonx ? "  " : "    ", f.out[i].name, f.out[i].name, pointee);
          else buf_printf (&outs, "%sif (%s) *%s = (%s) _r[%d];", f.nonx ? "  " : "    ", f.out[i].name, f.out[i].name, pointee, f.out[i].reg);
          free (ty); free (pointee);
        }
      if (f.nout && f.nonx) { buf_adds (&L, outs.s); buf_adds (&L, "\n"); }
      else if (f.nout) { buf_adds (&L, "  if (!_e)\n    {\n"); buf_adds (&L, outs.s); buf_adds (&L, "\n    }\n"); }
      if (!f.nonx) buf_adds (&L, "  return _e;\n}");
      else
        {
          if (strcmp (f.rtype, "void") != 0)
            {
              if (f.ret_reg == 16) buf_printf (&L, "  return (%s) _flags;\n", f.rtype);
              else buf_printf (&L, "  return (%s) _r[%d];\n", f.rtype, f.ret_reg);
            }
          buf_adds (&L, "}");
        }
      for (j = 0; j < f.nparams; j++)
        if (!find_reg (f.in, f.nin, f.params[j].name) && !find_reg (f.out, f.nout, f.params[j].name) && !in_block (&f, f.params[j].name))
          die ("%s: the parameter '%s' has no register in the comment", f.name, f.params[j].name);
      if (fi) buf_adds (&body, "\n\n");
      buf_adds (&body, L.s);
      free (L.s); free (outs.s);
    }
  buf_adds (&out, "/* Generated by mkoslib.py from the OSLib headers.  DO NOT EDIT. */\n");
  buf_adds (&out, "#include \"kernel.h\"\n");
  for (i = 0; i < nheads; i++) buf_printf (&out, "#include \"oslib/%s\"\n", heads[i]);
  buf_adds (&out, "\nextern _kernel_oserror *__modlib_xswi (unsigned swi_x, unsigned *regs);\n");
  if (uses_flags) buf_adds (&out, "extern _kernel_oserror *__modlib_xswif (unsigned swi_x, unsigned *regs, unsigned *flags);\n");
  if (uses_raise) buf_adds (&out, "extern void __modlib_raise (const void *e);\n");
  buf_adds (&out, "\n");
  buf_adds (&out, body.s);
  buf_adds (&out, "\n");
  return out.s;
}

/* ---------------------------------------------------------------- the undefined symbols of object files */
/* the global symbols that are undefined in an ELF file, added to NAMES (sorted by name, each once) */
static void undefined_in_elf (Elf *e, char ***names, int *n)
{
  char **mine = NULL;
  int nm = 0, i, k;
  for (i = 0; i < e->nsec; i++)
    {
      const ElfSec *y = &e->sec[i];
      size_t j;
      if (y->type != SHT_SYMTAB) continue;
      for (j = 16; j + 16 <= y->size; j += 16)                    /* entry 0 is the null symbol */
        {
          const unsigned char *p = e->data + y->offset + j;
          unsigned info = p[12];
          if (rd16 (p + 14) != 0 || (info >> 4) != 1 || rd32 (p) == 0) continue;     /* defined, or not a global symbol, or no name */
          mine = xrealloc (mine, sizeof (char *) * (size_t) (nm + 1));
          mine[nm++] = xstrdup (elf_symname (e, y, rd32 (p)));
        }
    }
  qsort (mine, (size_t) nm, sizeof (char *), cmp_str);
  for (k = 0; k < nm; k++)
    {
      int dup = 0;
      for (i = 0; i < *n; i++) if (strcmp ((*names)[i], mine[k]) == 0) dup = 1;
      if (!dup)
        {
          *names = xrealloc (*names, sizeof (char *) * (size_t) (*n + 1));
          (*names)[(*n)++] = mine[k];
        }
    }
}

/* the global symbols that an ELF file defines (a function that an object of the module has itself needs no veneer) */
static void defined_in_elf (Elf *e, char ***names, int *n)
{
  int i;
  for (i = 0; i < e->nsec; i++)
    {
      const ElfSec *y = &e->sec[i];
      size_t j;
      if (y->type != SHT_SYMTAB) continue;
      for (j = 16; j + 16 <= y->size; j += 16)
        {
          const unsigned char *p = e->data + y->offset + j;
          unsigned info = p[12];
          if (rd16 (p + 14) == 0 || (info >> 4) == 0 || rd32 (p) == 0) continue;     /* undefined, or local, or no name */
          *names = xrealloc (*names, sizeof (char *) * (size_t) (*n + 1));
          (*names)[(*n)++] = xstrdup (elf_symname (e, y, rd32 (p)));
        }
    }
}

/* An object file, or a static library (ar archive; its members that are ELF files).  Returns 1 for an archive.  A library may name functions that no member of it that gets linked needs, so what it
   uses is "soft": if the veneer of a function from a library cannot be made, that is a warning, not an error (the link says whether the function was needed). */
static int undefined_in (const char *path, char ***names, int *n, char ***defs, int *ndefs)
{
  size_t len;
  unsigned char *data = read_file (path, &len);
  if (len >= 8 && memcmp (data, "!<arch>\n", 8) == 0)
    {
      size_t pos = 8;
      while (pos + 60 <= len)
        {
          const unsigned char *h = data + pos;
          char sz[11];
          size_t size;
          memcpy (sz, h + 48, 10);
          sz[10] = 0;
          size = (size_t) strtoul (sz, NULL, 10);
          pos += 60;
          if (pos + size > len) break;
          if (size >= 52 && memcmp (data + pos, "\177ELF", 4) == 0)
            {
              Elf e;
              elf_load_mem (&e, data + pos, size, path);
              undefined_in_elf (&e, names, n);
              free (e.sec);
            }
          pos += size + (size & 1);
        }
      return 1;
    }
  {
    Elf e;
    elf_load_mem (&e, data, len, path);
    undefined_in_elf (&e, names, n);
    if (defs) defined_in_elf (&e, defs, ndefs);
    free (e.sec);
  }
  return 0;
}

/* 1 if the veneer of the function can be made, else 0 and die_message says why */
static int can_make (const char *dir, char *name)
{
  jmp_buf jb;
  volatile int ok = 0;
  die_recover = &jb;
  if (setjmp (jb) == 0)
    {
      char *one[1];
      one[0] = name;
      (void) generate (dir, one, 1);
      ok = 1;
    }
  die_recover = NULL;
  return ok;
}

static int declared_somewhere (const char *dir, const char *name);
static int is_nonx (const char *dir, const char *name)
{
  char *x;
  int r;
  if (declared_somewhere (dir, name)) return 0;
  x = xmalloc (strlen (name) + 2);
  x[0] = 'x'; strcpy (x + 1, name);
  r = declared_somewhere (dir, x);
  free (x);
  return r;
}

static int declared_somewhere (const char *dir, const char *name)
{
  Decl d;
  const char *us = strchr (name, '_');
  char *hdr;
  if (name[0] != 'x') return 0;
  if (us && us > name + 1)
    {
      hdr = xmalloc ((size_t) (us - name) + 4);
      memcpy (hdr, name + 1, (size_t) (us - name - 1));
      strcpy (hdr + (us - name - 1), ".h");
      if (try_header (dir, hdr, name, &d)) { free (hdr); return 1; }
      free (hdr);
    }
#if !defined (__riscos__)
  if (scan_all (dir, name, &d)) return 1;
#endif
  return 0;
}

#if !defined (__riscos__)
/* every X function that the headers declare: "extern os_error *xNAME (" at the start of a line; sorted by name, each once */
static void collect_names (const char *dir, char ***names, int *n)
{
  DIR *dp = opendir (dir);
  struct dirent *e;
  char **hdrs = NULL;
  int nh = 0, i, k;
  if (!dp) die ("cannot read the folder %s", dir);
  while ((e = readdir (dp)) != NULL)
    {
      size_t l = strlen (e->d_name);
      if (l > 2 && strcmp (e->d_name + l - 2, ".h") == 0) { hdrs = xrealloc (hdrs, sizeof (char *) * (size_t) (nh + 1)); hdrs[nh++] = xstrdup (e->d_name); }
    }
  closedir (dp);
  for (i = 0; i < nh; i++)
    {
      const char *p = (const char *) cached_text (dir, hdrs[i]);
      static const char pre[] = "extern os_error *";
      if (!p) continue;
      for (;;)
        {
          if (strncmp (p, pre, sizeof pre - 1) == 0 && p[sizeof pre - 1] == 'x')
            {
              const char *q = p + sizeof pre - 1;
              const char *w = q + 1;
              while (is_word ((unsigned char) *w)) w++;
              if (w > q + 1 && w[0] == ' ' && w[1] == '(')
                {
                  char *nm = xstrndup (q, (size_t) (w - q));
                  for (k = 0; k < *n; k++) if (strcmp ((*names)[k], nm) == 0) break;
                  if (k < *n) free (nm);
                  else { *names = xrealloc (*names, sizeof (char *) * (size_t) (*n + 1)); (*names)[(*n)++] = nm; }
                }
            }
          p = strchr (p, '\n');
          if (!p) break;
          p++;
        }
    }
  qsort (*names, (size_t) *n, sizeof (char *), cmp_str);
}

/* the text of the veneer of NAME, or NULL when it cannot be made (die_message says why) */
static char *try_generate (const char *dir, char *name)
{
  jmp_buf jb;
  char *volatile text = NULL;
  die_recover = &jb;
  if (setjmp (jb) == 0)
    {
      char *one[1];
      one[0] = name;
      text = generate (dir, one, 1);
    }
  die_recover = NULL;
  return text;
}

/* --library DIR: a C file for every OSLib function (X and non-X) that can be made, DIR/NAME.c, for libOSLib32.a (bin/mkoslib-lib.sh) */
static int library (const char *inc, const char *outdir)
{
  char **names = NULL;
  int n = 0, i, w, made = 0, skipped = 0;
  collect_names (inc, &names, &n);
  if (mkdir (outdir, 0777) != 0 && errno != EEXIST) die ("cannot make the folder %s", outdir);
  for (i = 0; i < n; i++)
    for (w = 0; w < 2; w++)
      {
        char *name = names[i] + w;
        char *text = try_generate (inc, name);
        if (!text) { fprintf (stderr, "skipped %s: %s\n", name, die_message); skipped++; continue; }
        {
          Buf path;
          buf_init (&path);
          buf_printf (&path, "%s/%s.c", outdir, name);
          write_file (path.s, text, strlen (text));
          free (path.s);
        }
        free (text);
        made++;
      }
  printf ("%s: %d functions made, %d skipped\n", outdir, made, skipped);
  return 0;
}
#else
static int library (const char *inc, const char *outdir) { (void) inc; (void) outdir; die ("--library is not available here"); return 1; }
#endif

static void usage (FILE *f)
{
  fputs ("usage: mkoslib [-I OSLIB_INCLUDE_DIR] -o OUT.c [FUNCTION ...] [--from-objects A.o [B.o ...]]\n       mkoslib [-I OSLIB_INCLUDE_DIR] --library DIR\n", f);
}

int main (int argc, char **argv)
{
  const char *inc = NULL, *outpath = NULL;
  char **funcs = xmalloc (sizeof (char *) * (size_t) (argc + 1));
  int nfuncs = 0, i, have_objs = 0;
  char **objs = xmalloc (sizeof (char *) * (size_t) (argc + 1));
  int nobjs = 0;
  char *text;
  const char *libdir = NULL;
  progname = "mkoslib";
  for (i = 1; i < argc; i++)
    {
      if (!strcmp (argv[i], "-I")) { if (++i >= argc) die ("-I needs a folder"); inc = argv[i]; }
      else if (!strcmp (argv[i], "-o")) { if (++i >= argc) die ("-o needs a file name"); outpath = argv[i]; }
      else if (!strcmp (argv[i], "--from-objects")) have_objs = 1;
      else if (!strcmp (argv[i], "--library")) { if (++i >= argc) die ("--library needs a folder"); libdir = argv[i]; }
      else if (!strcmp (argv[i], "-h") || !strcmp (argv[i], "--help")) { usage (stdout); return 0; }
      else if (argv[i][0] == '-' && argv[i][1]) { usage (stderr); die ("unknown option %s", argv[i]); }
      else if (have_objs) objs[nobjs++] = argv[i];
      else funcs[nfuncs++] = argv[i];
    }
  if (!outpath && !libdir) { usage (stderr); die ("-o OUT.c is required"); }
  if (!inc)
    {
      const char *o = getenv ("OSLIB");
      Buf b;
      if (!o || !*o) die ("no OSLib headers: give -I <the oslib folder>, or set OSLIB to the folder that has oslib/");
      buf_init (&b);
      buf_printf (&b, "%s/oslib", o);
      inc = b.s;
    }
  if (libdir) return library (inc, libdir);
  if (have_objs && !nobjs) die ("--from-objects needs object files");
  if (nobjs)
    {
      char **used = NULL, **soft = NULL, **defs = NULL;
      int nused = 0, nsoft = 0, ndefs = 0, k;
      for (i = 0; i < nobjs; i++)
        {
          if (undefined_in (objs[i], &soft, &nsoft, NULL, NULL))      /* a library: its names are soft */
            continue;
          undefined_in (objs[i], &used, &nused, &defs, &ndefs);
        }
      for (k = 0; k < nused; k++)                                     /* what one of the objects defines is not undefined for the link */
        {
          int j;
          for (j = 0; j < ndefs; j++) if (strcmp (used[k], defs[j]) == 0) { used[k] = (char *) ""; break; }
        }
      for (k = 0; k < nsoft; k++)
        {
          int j;
          for (j = 0; j < ndefs; j++) if (strcmp (soft[k], defs[j]) == 0) { soft[k] = (char *) ""; break; }
        }
      funcs = xrealloc (funcs, sizeof (char *) * (size_t) (nfuncs + nused + nsoft + 1));
      for (k = 0; k < nused; k++)
        {
          int dup = 0, j;
          if (!declared_somewhere (inc, used[k]) && !is_nonx (inc, used[k])) continue;
          for (j = 0; j < nfuncs; j++) if (strcmp (funcs[j], used[k]) == 0) dup = 1;
          if (!dup) funcs[nfuncs++] = used[k];
        }
      for (k = 0; k < nsoft; k++)
        {
          int dup = 0, j;
          if (!declared_somewhere (inc, soft[k]) && !is_nonx (inc, soft[k])) continue;
          for (j = 0; j < nfuncs; j++) if (strcmp (funcs[j], soft[k]) == 0) dup = 1;
          if (dup) continue;
          if (can_make (inc, soft[k])) funcs[nfuncs++] = soft[k];
          else fprintf (stderr, "mkoslib: warning: %s (used by a library) is not made: %s\n", soft[k], die_message);
        }
    }
  if (!nfuncs && !have_objs) die ("no function to make (name them, or give --from-objects)");
  if (nfuncs) text = generate (inc, funcs, nfuncs);
  else text = xstrdup ("/* Generated by mkoslib.py: the objects use no OSLib function. */\n");
  write_file (outpath, text, strlen (text));
  {
    Buf names;
    buf_init (&names);
    for (i = 0; i < nfuncs; i++) { if (i) buf_addc (&names, ' '); buf_adds (&names, funcs[i]); }
    printf ("%s: %d veneers: %s\n", outpath, nfuncs, names.s);
  }
  return 0;
}
