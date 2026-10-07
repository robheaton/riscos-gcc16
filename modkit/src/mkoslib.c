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
#if !defined (__riscos__)
#include <dirent.h>
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

static int try_header (const char *dir, const char *hdr, const char *name, Decl *d)
{
  int i;
  size_t pos;
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
  int has_r0op; int r0op_is_or; unsigned r0val;
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

static Func parse_func (const char *dir, const char *name)
{
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
                f.params = xrealloc (f.params, sizeof (Param) * (size_t) (f.nparams + 1));
                f.params[f.nparams].type = ty;
                f.params[f.nparams].name = xstrdup (p + nstart);
                f.params[f.nparams].text = p;
                f.nparams++;
              }
              cur.len = 0; cur.s[0] = 0;
              if (endp) break;
            }
          else buf_addc (&cur, ch);
        }
    }
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
      if (match_reg_line (l, "Input:", "entry", 0, &nm, &reg)) set_reg (&f.in, &f.nin, nm, reg);
      else if (match_reg_line (l, "Output:", "exit", 1, &nm, &reg)) set_reg (&f.out, &f.nout, nm, reg);
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
    if (strncmp (e, " with R0 ", 9) == 0)
      {
        const char *q = e + 9;
        int is_or = 0;
        if (strncmp (q, "|= ", 3) == 0) { is_or = 1; q += 3; }
        else if (strncmp (q, "= ", 2) == 0) q += 2;
        else q = NULL;
        if (q && q[0] == '0' && q[1] == 'x' && isxdigit ((unsigned char) q[2]))
          {
            f.has_r0op = 1; f.r0op_is_or = is_or;
            f.r0val = parse_hex_at (q, &e);
          }
      }
  }
  (void) i;
  free (comment); free (decl); free (norm); free (plist);
  return f;
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

static char *generate (const char *dir, char **funcs, int nfuncs)
{
  Buf out, body;
  char **heads = NULL;
  int nheads = 0, fi, i, j, flags, uses_flags = 0;
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
      buf_printf (&L, "os_error *%s (", f.name);
      if (!f.nparams) buf_adds (&L, "void");
      for (i = 0; i < f.nparams; i++) { if (i) buf_adds (&L, ", "); buf_adds (&L, f.params[i].text); }
      for (i = 0; i < f.nout; i++) if (f.out[i].reg == 16) flags = 1;
      uses_flags |= flags;
      buf_adds (&L, ")\n{\n  unsigned r[10] = { 0 };\n");
      if (flags) buf_adds (&L, "  unsigned flags = 0;\n");
      sort_regs (f.in, f.nin);
      for (i = 0; i < f.nin; i++)
        {
          if (!find_param (&f, f.in[i].name)) die ("%s: the comment names an input '%s' that is not a parameter", f.name, f.in[i].name);
          buf_printf (&L, "  r[%d] = (unsigned) %s;\n", f.in[i].reg, f.in[i].name);
        }
      if (f.has_r0op) buf_printf (&L, "  r[0] %s %s;\n", f.r0op_is_or ? "|=" : "=", hx (f.r0val));
      buf_printf (&L, "  os_error *e = (os_error *) %s (%s, r%s);\n", flags ? "__modlib_xswif" : "__modlib_xswi", hx (0x20000u | f.swi), flags ? ", &flags" : "");
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
          if (f.out[i].reg == 16) buf_printf (&outs, "    if (%s) *%s = (%s) flags;", f.out[i].name, f.out[i].name, pointee);
          else buf_printf (&outs, "    if (%s) *%s = (%s) r[%d];", f.out[i].name, f.out[i].name, pointee, f.out[i].reg);
          free (ty); free (pointee);
        }
      if (f.nout) { buf_adds (&L, "  if (!e)\n    {\n"); buf_adds (&L, outs.s); buf_adds (&L, "\n    }\n"); }
      buf_adds (&L, "  return e;\n}");
      for (j = 0; j < f.nparams; j++)
        if (!find_reg (f.in, f.nin, f.params[j].name) && !find_reg (f.out, f.nout, f.params[j].name))
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

static int declared_somewhere (const char *dir, const char *name)
{
  Decl d;
  const char *us = strchr (name, '_');
  char *hdr;
  if (name[0] != 'x' || !us || us <= name + 1) return 0;
  hdr = xmalloc ((size_t) (us - name) + 4);
  memcpy (hdr, name + 1, (size_t) (us - name - 1));
  strcpy (hdr + (us - name - 1), ".h");
  if (try_header (dir, hdr, name, &d)) { free (hdr); return 1; }
  free (hdr);
#if !defined (__riscos__)
  if (scan_all (dir, name, &d)) return 1;
#endif
  return 0;
}

static void usage (FILE *f)
{
  fputs ("usage: mkoslib [-I OSLIB_INCLUDE_DIR] -o OUT.c [FUNCTION ...] [--from-objects A.o [B.o ...]]\n", f);
}

int main (int argc, char **argv)
{
  const char *inc = NULL, *outpath = NULL;
  char **funcs = xmalloc (sizeof (char *) * (size_t) (argc + 1));
  int nfuncs = 0, i, have_objs = 0;
  char **objs = xmalloc (sizeof (char *) * (size_t) (argc + 1));
  int nobjs = 0;
  char *text;
  progname = "mkoslib";
  for (i = 1; i < argc; i++)
    {
      if (!strcmp (argv[i], "-I")) { if (++i >= argc) die ("-I needs a folder"); inc = argv[i]; }
      else if (!strcmp (argv[i], "-o")) { if (++i >= argc) die ("-o needs a file name"); outpath = argv[i]; }
      else if (!strcmp (argv[i], "--from-objects")) have_objs = 1;
      else if (!strcmp (argv[i], "-h") || !strcmp (argv[i], "--help")) { usage (stdout); return 0; }
      else if (argv[i][0] == '-' && argv[i][1]) { usage (stderr); die ("unknown option %s", argv[i]); }
      else if (have_objs) objs[nobjs++] = argv[i];
      else funcs[nfuncs++] = argv[i];
    }
  if (!outpath) { usage (stderr); die ("-o OUT.c is required"); }
  if (!inc)
    {
      const char *o = getenv ("OSLIB");
      Buf b;
      if (!o || !*o) die ("no OSLib headers: give -I <the oslib folder>, or set OSLIB to the folder that has oslib/");
      buf_init (&b);
      buf_printf (&b, "%s/oslib", o);
      inc = b.s;
    }
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
          if (!declared_somewhere (inc, used[k])) continue;
          for (j = 0; j < nfuncs; j++) if (strcmp (funcs[j], used[k]) == 0) dup = 1;
          if (!dup) funcs[nfuncs++] = used[k];
        }
      for (k = 0; k < nsoft; k++)
        {
          int dup = 0, j;
          if (!declared_somewhere (inc, soft[k])) continue;
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
