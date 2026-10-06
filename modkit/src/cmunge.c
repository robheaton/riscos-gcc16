/* cmunge - CMunge's command line over the module header generator of modkit: the C version of bin/cmunge and bin/mkmodhdr.py (the same output, byte for byte).

     cmunge [-tgcc] [-32bit] [-p|-px] [-D<sym>[=<val>]] [-U<sym>] [-I<dir>] [-throwback] [-s FILE.s] [-d FILE.h] [-o FILE.o] FILE.cmhg

Reads a CMHG file and writes
  -s  the module header and the veneers as GNU assembler source        -d  the C header (Module_Title, CMD_<name>, the prototypes of your handlers)
  -o  the object: the assembler source assembled for ARMv6 by the compiler (CMUNGE_CC, else the cross compiler next to this program or on PATH; on RISC OS: gcc)
  -p, -px  run the C preprocessor over the file first (-D, -U and -I go to it)
Accepted and without effect (the model has nothing to switch): -tgcc, -32bit, -znoscl, -throwback, -cmhg.  What CMunge has beyond that (-tnorcroft, -tlcc, -26bit, -apcs, -zbase, -zerrors,
-zoslib, -blank, -x<type>, -depend) is refused with a message, not ignored: a module that needs it would be built wrong.

The CMHG language that is supported (anything else is an error, nothing is dropped silently):
  title-string:, help-string: (name and version), date-string:        the module's name, help line (title <tab> version (date)) and the Module_* macros of the C header
  initialisation-code: FN          _kernel_oserror *FN (const char *tail, int podule_base, void *pw)
  finalisation-code: FN            _kernel_oserror *FN (int fatal, int podule_base, void *pw)
  service-call-handler: FN [N ...] void FN (int service_number, _kernel_swi_regs *r, void *pw);  with service numbers the kernel only calls it for those; to claim a call the handler sets r->r[1] = 0
  command-keyword-table: FN        name (min-args: n, max-args: m, gstrans-map: bits, help-text: "...", invalid-syntax: "...")   -  _kernel_oserror *FN (const char *arg_string, int argc, int number, void *pw)
  swi-chunk-base-number: N, swi-decoding-table: PREFIX NAME ..., swi-handler-code: FN     _kernel_oserror *FN (int swi_offset, _kernel_swi_regs *r, void *pw)
  irq-handlers:, vector-handlers:, generic-veneers: ENTRY/FN, ...     int FN (_kernel_swi_regs *r, void *pw)
  module-is-runnable:              ignored with a warning (there is no start code in this model)
The generated code relocates the image once, in its initialisation (modreloc appends the table of address words; see modkit/README.md). */
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include "modcommon.h"

/* ---------------------------------------------------------------- the module description */
typedef struct
{
  char *name;
  unsigned min, max, gstrans, flags;
  unsigned char *help; size_t helplen; int has_help;
  unsigned char *syntax; size_t synlen; int has_syntax;
} Cmd;

typedef struct { char *kind, *entry, *handler; } Veneer;

typedef struct
{
  char *title, *help, *date, *init, *final, *service;
  unsigned *svc; int nsvc;
  Cmd *cmds; int ncmds; char *cmd_handler;
  unsigned swi_chunk; char *swi_prefix; char **swi_names; int nswi; char *swi_handler;
  Veneer *ven; int nven;
  int runnable;
} Module;

static const char *srcname;                  /* the CMHG file, for messages */

static void cmhg_error (const char *fmt, ...) __attribute__ ((noreturn, format (printf, 1, 2)));
static void cmhg_error (const char *fmt, ...)
{
  va_list ap;
  fprintf (stderr, "%s: %s: ", progname, srcname);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  exit (1);
}

/* ---------------------------------------------------------------- small scanners (ASCII: what the CMHG files use) */
static int is_space (int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }
static int is_word (int c) { return isalnum (c) || c == '_'; }
static int is_alpha (int c) { return isalpha (c); }

static char *strip (const char *s)                        /* Python's str.strip () */
{
  size_t a = 0, b = strlen (s);
  while (a < b && is_space ((unsigned char) s[a])) a++;
  while (b > a && is_space ((unsigned char) s[b - 1])) b--;
  return xstrndup (s + a, b - a);
}

/* the words of S separated by white space and (when COMMAS) commas, like Python's split () / re.split (r"[\s,]+") without the empty ones */
static char **split_words (const char *s, int commas, int *n)
{
  char **w = NULL;
  int k = 0;
  const char *p = s;
  for (;;)
    {
      const char *q;
      while (*p && (is_space ((unsigned char) *p) || (commas && *p == ','))) p++;
      if (!*p) break;
      q = p;
      while (*q && !is_space ((unsigned char) *q) && !(commas && *q == ',')) q++;
      w = xrealloc (w, sizeof (char *) * (size_t) (k + 1));
      w[k++] = xstrndup (p, (size_t) (q - p));
      p = q;
    }
  *n = k;
  return w;
}

static unsigned parse_int (const char *s)
{
  char *end;
  unsigned long v;
  if (s[0] == '&') v = strtoul (s + 1, &end, 16);
  else if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) v = strtoul (s + 2, &end, 16);
  else v = strtoul (s, &end, 10);
  if (end == s || *end) cmhg_error ("%s is not a number", s);
  return (unsigned) v;
}

/* ---------------------------------------------------------------- the CMHG parser */
typedef struct { char *key; char *rest; } Line;

static Line *logical_lines (const char *text, int *nlines)
{
  Line *out = NULL;
  int n = 0;
  Buf *cur = NULL;
  Buf *bufs = NULL;
  const char *p = text;
  for (;;)
    {
      const char *nl = strchr (p, '\n');
      size_t len = nl ? (size_t) (nl - p) : strlen (p);
      char *line;
      const char *l;
      while (len && p[len - 1] == '\r') len--;
      line = xstrndup (p, len);
      l = line;
      while (*l && is_space ((unsigned char) *l)) l++;
      if (*l && *l != ';')
        {
          if ((line[0] == ' ' || line[0] == '\t') && cur)
            {
              buf_addc (cur, '\n');
              buf_adds (cur, line);
            }
          else
            {
              size_t k = 0, j;
              if (!is_alpha ((unsigned char) line[0])) cmhg_error ("cannot parse the line: '%s'", line);
              k = 1;
              while (is_word ((unsigned char) line[k]) || line[k] == '-') k++;
              j = k;
              while (line[j] && is_space ((unsigned char) line[j])) j++;
              if (line[j] != ':') cmhg_error ("cannot parse the line: '%s'", line);
              j++;
              while (line[j] && is_space ((unsigned char) line[j])) j++;
              out = xrealloc (out, sizeof (Line) * (size_t) (n + 1));
              bufs = xrealloc (bufs, sizeof (Buf) * (size_t) (n + 1));
              out[n].key = xstrndup (line, k);
              { char *c; for (c = out[n].key; *c; c++) *c = (char) tolower ((unsigned char) *c); }
              buf_init (&bufs[n]);
              buf_adds (&bufs[n], line + j);
              n++;
              cur = &bufs[n - 1];
            }
        }
      free (line);
      if (!nl) break;
      p = nl + 1;
    }
  { int i; for (i = 0; i < n; i++) out[i].rest = bufs[i].s; }
  free (bufs);
  *nlines = n;
  return out;
}

static unsigned char escape_char (char e)
{
  switch (e)
    {
    case 'n': case 'r': return 13;
    case 't': return 9;
    case '\\': return 92;
    case '"': return 34;
    case '\'': return 39;
    case 'a': return 7;
    case 'b': return 8;
    case 'f': return 12;
    case 'v': return 11;
    case '0': return 0;
    }
  cmhg_error ("unknown escape \\%c in a string", e);
}

static int hexval (int c) { return isdigit (c) ? c - '0' : (tolower (c) - 'a' + 10); }

/* one or more adjacent string literals starting at POS (after blanks): CMHG's \n is the RISC OS line end 13 */
static unsigned char *parse_string_literals (const char *s, size_t *pos, size_t *outlen)
{
  Buf b;
  int first = 1;
  size_t p = *pos, len = strlen (s);
  buf_init (&b);
  for (;;)
    {
      while (p < len && (s[p] == ' ' || s[p] == '\t' || s[p] == '\n' || s[p] == '\r')) p++;
      if (p >= len || s[p] != '"') break;
      p++;
      while (p < len && s[p] != '"')
        {
          char c = s[p];
          if (c == '\\')
            {
              char e;
              p++;
              if (p >= len) cmhg_error ("unterminated string");
              e = s[p];
              if (e == 'x')
                {
                  if (p + 2 >= len || !isxdigit ((unsigned char) s[p + 1]) || !isxdigit ((unsigned char) s[p + 2])) cmhg_error ("a \\x escape needs two hex digits");
                  buf_addc (&b, (char) (hexval ((unsigned char) s[p + 1]) * 16 + hexval ((unsigned char) s[p + 2])));
                  p += 2;
                }
              else buf_addc (&b, (char) escape_char (e));
            }
          else buf_addc (&b, c);
          p++;
        }
      if (p >= len) cmhg_error ("unterminated string");
      p++;
      first = 0;
    }
  if (first) cmhg_error ("a string was expected near '%.20s'", s + p);
  *pos = p;
  *outlen = b.len;
  return (unsigned char *) b.s;
}

static void parse_command_table (const char *rest, Module *m)
{
  size_t pos = 0, len = strlen (rest), k;
  int n = 0;
  while (pos < len && is_space ((unsigned char) rest[pos])) pos++;
  k = pos;
  if (!(is_alpha ((unsigned char) rest[k]) || rest[k] == '_')) cmhg_error ("command-keyword-table: needs the name of the handler function");
  while (is_word ((unsigned char) rest[k])) k++;
  m->cmd_handler = xstrndup (rest + pos, k - pos);
  pos = k;
  while (pos < len && is_space ((unsigned char) rest[pos])) pos++;
  m->cmds = NULL;
  while (pos < len)
    {
      Cmd *c;
      while (pos < len && (is_space ((unsigned char) rest[pos]) || rest[pos] == ',')) pos++;
      if (pos >= len) break;
      k = pos;
      if (!(is_alpha ((unsigned char) rest[k]) || rest[k] == '_')) cmhg_error ("a command name was expected near '%.30s'", rest + pos);
      while (is_word ((unsigned char) rest[k]) || rest[k] == '$' || rest[k] == '%') k++;
      m->cmds = xrealloc (m->cmds, sizeof (Cmd) * (size_t) (n + 1));
      c = &m->cmds[n++];
      memset (c, 0, sizeof *c);
      c->name = xstrndup (rest + pos, k - pos);
      c->min = 0; c->max = 255;
      pos = k;
      while (pos < len && is_space ((unsigned char) rest[pos])) pos++;
      if (pos < len && rest[pos] == '(')
        {
          pos++;
          for (;;)
            {
              char *key;
              size_t j;
              while (pos < len && (is_space ((unsigned char) rest[pos]) || rest[pos] == ',')) pos++;
              if (pos >= len) cmhg_error ("the command options of %s are not closed", c->name);
              if (rest[pos] == ')') { pos++; break; }
              j = pos;
              if (!is_alpha ((unsigned char) rest[j])) cmhg_error ("a command option was expected near '%.30s'", rest + pos);
              while (is_word ((unsigned char) rest[j]) || rest[j] == '-') j++;
              key = xstrndup (rest + pos, j - pos);
              { char *q; for (q = key; *q; q++) *q = (char) tolower ((unsigned char) *q); }
              pos = j;
              while (pos < len && is_space ((unsigned char) rest[pos])) pos++;
              if (pos < len && rest[pos] == ':') pos++;
              while (pos < len && is_space ((unsigned char) rest[pos])) pos++;
              if (strcmp (key, "min-args") == 0 || strcmp (key, "max-args") == 0 || strcmp (key, "gstrans-map") == 0)
                {
                  size_t e = pos;
                  char *num;
                  if (rest[e] == '&' || (rest[e] == '0' && (rest[e + 1] == 'x' || rest[e + 1] == 'X')))
                    {
                      e += rest[e] == '&' ? 1 : 2;
                      if (!isxdigit ((unsigned char) rest[e])) cmhg_error ("a number was expected near '%.30s'", rest + pos);
                      while (isxdigit ((unsigned char) rest[e])) e++;
                    }
                  else
                    {
                      if (!isdigit ((unsigned char) rest[e])) cmhg_error ("a number was expected near '%.30s'", rest + pos);
                      while (isdigit ((unsigned char) rest[e])) e++;
                    }
                  num = xstrndup (rest + pos, e - pos);
                  pos = e;
                  if (key[0] == 'm' && key[1] == 'i') c->min = parse_int (num);
                  else if (key[0] == 'm') c->max = parse_int (num);
                  else c->gstrans = parse_int (num);
                  free (num);
                }
              else if (strcmp (key, "help-text") == 0)
                {
                  c->help = parse_string_literals (rest, &pos, &c->helplen);
                  c->has_help = 1;
                }
              else if (strcmp (key, "invalid-syntax") == 0)
                {
                  c->syntax = parse_string_literals (rest, &pos, &c->synlen);
                  c->has_syntax = 1;
                }
              else cmhg_error ("command option '%s' is not supported", key);
              free (key);
            }
        }
    }
  if (!n) cmhg_error ("the command-keyword-table has no commands");
  m->ncmds = n;
}

static void add_veneer (Module *m, const char *kind, const char *entry, const char *handler)
{
  m->ven = xrealloc (m->ven, sizeof (Veneer) * (size_t) (m->nven + 1));
  m->ven[m->nven].kind = xstrdup (kind);
  m->ven[m->nven].entry = xstrdup (entry);
  m->ven[m->nven].handler = xstrdup (handler);
  m->nven++;
}

static void parse_cmhg (const char *text, Module *m)
{
  int n, i;
  Line *lines = logical_lines (text, &n);
  memset (m, 0, sizeof *m);
  for (i = 0; i < n; i++)
    {
      const char *key = lines[i].key;
      char *r = strip (lines[i].rest);
      if (strcmp (key, "title-string") == 0) m->title = r;
      else if (strcmp (key, "help-string") == 0) m->help = r;
      else if (strcmp (key, "date-string") == 0) m->date = r;
      else if (strcmp (key, "initialisation-code") == 0) m->init = r;
      else if (strcmp (key, "finalisation-code") == 0) m->final = r;
      else if (strcmp (key, "service-call-handler") == 0)
        {
          int np, k;
          char **w = split_words (r, 1, &np);
          if (np < 1) cmhg_error ("service-call-handler: needs the name of the handler function");
          m->service = w[0];
          for (k = 1; k < np; k++)
            {
              m->svc = xrealloc (m->svc, sizeof (unsigned) * (size_t) (m->nsvc + 1));
              m->svc[m->nsvc++] = parse_int (w[k]);
            }
        }
      else if (strcmp (key, "command-keyword-table") == 0) parse_command_table (lines[i].rest, m);
      else if (strcmp (key, "swi-chunk-base-number") == 0) m->swi_chunk = parse_int (r);
      else if (strcmp (key, "swi-decoding-table") == 0)
        {
          int np, k;
          char **w = split_words (r, 1, &np);
          if (np < 1) cmhg_error ("swi-decoding-table: needs the prefix");
          m->swi_prefix = w[0];
          m->swi_names = np > 1 ? xmalloc (sizeof (char *) * (size_t) (np - 1)) : NULL;
          for (k = 1; k < np; k++) m->swi_names[k - 1] = w[k];
          m->nswi = np - 1;
        }
      else if (strcmp (key, "swi-handler-code") == 0) m->swi_handler = r;
      else if (strcmp (key, "irq-handlers") == 0 || strcmp (key, "vector-handlers") == 0 || strcmp (key, "generic-veneers") == 0)
        {
          int np, k;
          char **w = split_words (r, 1, &np);
          for (k = 0; k < np; k++)
            {
              char *slash = strchr (w[k], '/');
              if (!slash) cmhg_error ("%s: ENTRY/HANDLER was expected, got '%s'", key, w[k]);
              *slash = 0;
              add_veneer (m, key, w[k], slash + 1);
            }
        }
      else if (strcmp (key, "module-is-runnable") == 0)
        {
          m->runnable = 1;
          fprintf (stderr, "%s: warning: module-is-runnable: ignored (this model has no start code)\n", progname);
        }
      else cmhg_error ("%s: is not supported", key);
    }
  if (!m->title || !*m->title) cmhg_error ("title-string: is missing");
  if (!m->help || !*m->help) cmhg_error ("help-string: is missing");
}

/* ---------------------------------------------------------------- the generators */
/* the text of the assembler source is a list of items that are joined by line ends (Python: "\n".join (o) + "\n"); an item can have line ends of its own */
typedef struct { Buf b; int first; } Out;
static void out_init (Out *o) { buf_init (&o->b); o->first = 1; }
static void A (Out *o, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
static void A (Out *o, const char *fmt, ...)
{
  va_list ap;
  int n;
  char *tmp;
  va_start (ap, fmt);
  n = vsnprintf (NULL, 0, fmt, ap);
  va_end (ap);
  tmp = xmalloc ((size_t) n + 1);
  va_start (ap, fmt);
  vsnprintf (tmp, (size_t) n + 1, fmt, ap);
  va_end (ap);
  if (!o->first) buf_addc (&o->b, '\n');
  o->first = 0;
  buf_adds (&o->b, tmp);
  free (tmp);
}

/* a .ascii / .byte sequence for the bytes (printable runs as strings) */
static char *asm_bytes (const unsigned char *b, size_t n)
{
  Buf out, run;
  size_t i;
  int items = 0;
  buf_init (&out); buf_init (&run);
#define FLUSH() do { if (run.len) { if (items++) buf_addc (&out, '\n'); buf_adds (&out, "\t.ascii\t\"" ); buf_adds (&out, run.s); buf_adds (&out, "\""); run.len = 0; run.s[0] = 0; } } while (0)
  for (i = 0; i < n; i++)
    {
      unsigned char c = b[i];
      if (c >= 32 && c < 127)
        {
          if (c == '\\' || c == '"') buf_addc (&run, '\\');
          buf_addc (&run, (char) c);
        }
      else
        {
          FLUSH ();
          if (items++) buf_addc (&out, '\n');
          buf_printf (&out, "\t.byte\t%d", c);
        }
    }
  FLUSH ();
#undef FLUSH
  if (!items) return xstrdup ("\t.ascii\t\"\"");
  free (run.s);
  return out.s;
}

static const char *version_of (const Module *m)       /* the part of the help string after the title (the whole string when it does not start with it) */
{
  size_t tl = strlen (m->title);
  if (strncmp (m->help, m->title, tl) == 0) return m->help + tl;
  return m->help;
}

static int cmp_unsigned (const void *a, const void *b)
{
  unsigned x = *(const unsigned *) a, y = *(const unsigned *) b;
  return x < y ? -1 : x > y;
}

static char *generate_asm (const Module *m, const char *src)
{
  Out o;
  char *ver, *helpline, *hb;
  size_t tl = strlen (m->title);
  int has_svc = m->service != NULL, has_swi = m->swi_handler != NULL, i, k;
  int ncmds = m->ncmds;
  unsigned *nums = NULL;
  int nn = 0;
  Buf hl;

  ver = strip (version_of (m));
  buf_init (&hl);
  buf_adds (&hl, m->title);
  buf_adds (&hl, tl < 8 ? "\t\t" : "\t");
  buf_adds (&hl, strncmp (m->help, m->title, tl) == 0 ? ver : m->help);
  if (m->date && *m->date) { buf_adds (&hl, " ("); buf_adds (&hl, m->date); buf_adds (&hl, ")"); }
  helpline = hl.s;
  /* (Python: ver = help[len (title):].strip () if help starts with the title else help (unstripped)) */

  out_init (&o);
  A (&o, "@ Generated by mkmodhdr.py from %s.  The module header and the veneers of the module; see modkit/README.md.  DO NOT EDIT.", base_name (src));
  A (&o, "\t.syntax\tunified\n\t.arm");
  A (&o, "\t.equ\tXOS_SynchroniseCodeAreas, %s", hx (0x2006E));
  A (&o, "\t.section\t\".text.header\",\"ax\"\n\t.global\t_start\n_start:");
  A (&o, "\t.word\t0\t\t\t\t@ start code (none)");
  A (&o, "\t.word\tinit - _start\n\t.word\tfinal - _start");
  A (&o, "\t.word\t%s", has_svc ? "service - _start\t\t@ service call handler" : "0\t\t\t\t@ service call handler (none)");
  A (&o, "\t.word\ttitle - _start\n\t.word\thelp - _start");
  A (&o, "\t.word\t%s", ncmds ? "cmdtab - _start" : "0");
  A (&o, "\t.word\t%s\t\t\t@ SWI chunk base", hx (has_swi ? m->swi_chunk : 0));
  A (&o, "\t.word\t%s", has_swi ? "swi_entry - _start" : "0");
  A (&o, "\t.word\t%s", has_swi && m->swi_prefix ? "title - _start\t\t@ the decoding table shares the title string" : "0");
  A (&o, "\t.word\t0\t\t\t\t@ SWI decoding code\n\t.word\t0\t\t\t\t@ messages file\n\t.word\tflags - _start");
  A (&o, "title:\n\t.asciz\t\"%s\"", m->title);
  if (has_swi && m->swi_prefix)
    {
      const char *p = m->swi_prefix, *t = m->title;
      int same = strlen (p) == strlen (t);
      for (k = 0; same && p[k]; k++) if (tolower ((unsigned char) p[k]) != tolower ((unsigned char) t[k])) same = 0;
      if (!same) cmhg_error ("this model shares the title string with the SWI decoding table: the SWI prefix (%s) must be the title (%s)", m->swi_prefix, m->title);
      for (i = 0; i < m->nswi; i++) A (&o, "\t.asciz\t\"%s\"", m->swi_names[i]);
      A (&o, "\t.byte\t0\t\t\t\t@ end of the SWI table");
    }
  hb = asm_bytes ((const unsigned char *) helpline, strlen (helpline));
  A (&o, "help:\n%s\n\t.byte\t0\n\t.align\t2", hb);
  free (hb);
  /* command table */
  if (ncmds)
    {
      for (i = 0; i < ncmds; i++)
        {
          const Cmd *c = &m->cmds[i];
          if (c->has_help) { char *s = asm_bytes (c->help, c->helplen); A (&o, "ht%d:\n%s\n\t.byte\t0", i, s); free (s); }
          if (c->has_syntax) { char *s = asm_bytes (c->syntax, c->synlen); A (&o, "is%d:\n%s\n\t.byte\t0", i, s); free (s); }
        }
      A (&o, "\t.align\t2\ncmdtab:");
      for (i = 0; i < ncmds; i++)
        {
          const Cmd *c = &m->cmds[i];
          unsigned info = c->min | (c->gstrans << 8) | ((c->max < 255 ? c->max : 255) << 16) | (c->flags << 24);
          char buf[64];
          A (&o, "\t.asciz\t\"%s\"\n\t.align\t2", c->name);
          A (&o, "\t.word\tcmd%d - _start\t\t@ code", i);
          A (&o, "\t.word\t%s\t\t\t@ min %u, max %u parameters", hx (info), c->min, c->max);
          if (c->has_syntax) { sprintf (buf, "is%d - _start", i); A (&o, "\t.word\t%s", buf); }
          else A (&o, "\t.word\t0\t\t\t\t@ no invalid syntax line");
          if (c->has_help) { sprintf (buf, "ht%d - _start", i); A (&o, "\t.word\t%s", buf); }
          else A (&o, "\t.word\t0\t\t\t\t@ no help text");
        }
      A (&o, "\t.word\t0\t\t\t\t@ end of the table");
    }
  A (&o, "\t.align\t2\nflags:\t.word\t1\t\t\t\t@ bit 0: 32-bit compatible");
  A (&o, "link_addr:\n\t.word\t_start\t\t\t\t@ the linked address of the image (0): relocated with everything else");
  A (&o, "reloc_info:\n\t.word\t0\t\t\t\t@ offset of the relocation table (filled in by modreloc.py)\n\t.word\t0\t\t\t\t@ number of entries\n\t.ltorg");
  A (&o, "%s", "");
  A (&o, "@ ---- initialisation: relocate the image (once), then call the module's code.  r10 = environment string, r11 = podule base / instantiation, r12 = private word");
  A (&o, "\t.balign\t4\ninit:\n\tstmfd\tsp!, {r4-r11, lr}");
  A (&o, "\tadrl\tr4, _start\t\t\t@ where the image is now (PC relative)\n\tldr\tr5, link_addr\t\t\t@ where the linker put it (0), or where it already is (a second initialisation)\n\tsubs\tr6, r4, r5\n\tbeq\trelocated");
  A (&o, "\tadrl\tr7, reloc_info\n\tldr\tr8, [r7]\n\tldr\tr9, [r7, #4]\n\tadd\tr8, r4, r8\nrloop:\tcmp\tr9, #0\n\tbeq\trdone\n\tldr\tr0, [r8], #4\n\tldr\tr1, [r4, r0]\n\tadd\tr1, r1, r6\n\tstr\tr1, [r4, r0]\n\tsub\tr9, r9, #1\n\tb\trloop");
  A (&o, "rdone:\tmov\tr0, #1\n\tmov\tr1, r4\n\tldr\tr2, =__image_end\n\tsub\tr2, r2, #1\n\tswi\tXOS_SynchroniseCodeAreas\nrelocated:");
  if (m->init) A (&o, "\tmov\tr0, r10\n\tmov\tr1, r11\n\tmov\tr2, r12\n\tmov\tr4, sp\n\tbic\tsp, sp, #7\n\tbl\t%s\n\tmov\tsp, r4\n\tb\tdone", m->init);
  else A (&o, "\tmov\tr0, #0\n\tb\tdone");
  A (&o, "\nfinal:\n\tstmfd\tsp!, {r4-r11, lr}");
  if (m->final) A (&o, "\tmov\tr0, r10\n\tmov\tr1, r11\n\tmov\tr2, r12\n\tmov\tr4, sp\n\tbic\tsp, sp, #7\n\tbl\t%s\n\tmov\tsp, r4\n\tb\tdone", m->final);
  else A (&o, "\tmov\tr0, #0\n\tb\tdone");
  /* commands */
  if (ncmds)
    {
      A (&o, "%s", "");
      for (i = 0; i < ncmds; i++) A (&o, "cmd%d:\n\tmov\tr2, #%d\n\tb\tcmd_common", i, i);
      A (&o, "cmd_common:\t\t\t\t\t@ r0 = argument string, r1 = number of parameters, r2 = the number of the command, r12 = private word\n\tstmfd\tsp!, {r4-r11, lr}\n\tmov\tr3, r12\n\tmov\tr5, r0\n\tmov\tr4, sp\n\tbic\tsp, sp, #7\n\tbl\t%s\n\tmov\tsp, r4", m->cmd_handler);
      A (&o, "\tcmp\tr0, #0\n\tbeq\tcmd_ok\n\tcmp\tr0, r5\t\t\t\t@ the handler gave back the argument string: help_PRINT_BUFFER\n\tbeq\tcmd_ok\n\tcmn\tr0, #1\n\tbeq\tcmd_ok\n\tmov\tr1, #0\n\tcmp\tr1, #0x80000000\t\t\t@ V set: an error, r0 = the error block\n\tldmfd\tsp!, {r4-r11, pc}\ncmd_ok:\tmov\tr0, #0\n\tcmp\tr0, #0\n\tldmfd\tsp!, {r4-r11, pc}");
    }
  /* service calls */
  if (has_svc)
    {
      A (&o, "%s", "");
      if (m->nsvc)
        {
          nums = xmalloc (sizeof (unsigned) * (size_t) m->nsvc);
          memcpy (nums, m->svc, sizeof (unsigned) * (size_t) m->nsvc);
          qsort (nums, (size_t) m->nsvc, sizeof (unsigned), cmp_unsigned);
          for (i = 0; i < m->nsvc; i++) if (i == 0 || nums[i] != nums[i - 1]) nums[nn++] = nums[i];
        }
      if (nn)
        {
          A (&o, "\t.balign\t4\nservtab:\n\t.word\t0\t\t\t\t@ flags\n\t.word\tursservice - _start");
          for (i = 0; i < nn; i++) A (&o, "\t.word\t%s", hx (nums[i]));
          A (&o, "\t.word\t0\t\t\t\t@ end of the table\n\t.word\tservtab - _start\t\t@ the anchor: the word before the entry point");
        }
      A (&o, "service:\n\tmov\tr0, r0\t\t\t\t@ the magic instruction for the kernel's service call despatcher");
      if (nn)
        {
          Buf t;
          buf_init (&t);
          buf_adds (&t, "\t");
          for (i = 0; i < nn; i++)
            {
              if (i) buf_adds (&t, "\n\t");
              buf_printf (&t, "%s\tr1, #%s", i == 0 ? "teq" : "teqne", hx (nums[i]));
            }
          buf_adds (&t, "\n\tmovne\tpc, lr");
          A (&o, "%s", t.s);
          free (t.s);
        }
      A (&o, "ursservice:\n\tstmfd\tsp!, {r0-r11, lr}\n\tmov\tr0, r1\t\t\t\t@ service number\n\tmov\tr1, sp\t\t\t\t@ the registers r0 - r9 as a block\n\tmov\tr2, r12\n\tmov\tr4, sp\n\tbic\tsp, sp, #7\n\tbl\t%s\n\tmov\tsp, r4\n\tldmfd\tsp!, {r0-r11, pc}", m->service);
    }
  /* SWIs */
  if (has_swi)
    {
      A (&o, "%s", "");
      A (&o, "swi_entry:\t\t\t\t\t@ r11 = SWI number - chunk base, r0 - r9 = the SWI's registers, r12 = private word\n\tstmfd\tsp!, {r0-r9, lr}\n\tmov\tr0, r11\n\tmov\tr1, sp\n\tmov\tr2, r12\n\tmov\tr4, sp\n\tbic\tsp, sp, #7\n\tbl\t%s\n\tmov\tsp, r4\n\tcmp\tr0, #0\n\tbne\tswi_err\n\tldmfd\tsp!, {r0-r9, pc}\nswi_err:\n\tadd\tsp, sp, #4\n\tldmfd\tsp!, {r1-r9, lr}\n\tmsr\tcpsr_f, #0x10000000\n\tmov\tpc, lr", m->swi_handler);
    }
  /* vector / IRQ / generic veneers */
  for (i = 0; i < m->nven; i++)
    {
      const Veneer *v = &m->ven[i];
      A (&o, "%s", "");
      A (&o, "\t.global\t%s\n%s:\t\t\t\t\t\t@ %s: r12 = private word; for a vector lr = the pass-on address and the kernel stacked the claim address", v->entry, v->entry, v->kind);
      A (&o, "\tstmfd\tsp!, {r0-r11, lr}\n\tmov\tr0, sp\t\t\t\t@ the registers as a block\n\tmov\tr1, r12\n\tmrs\tr6, cpsr\n\torr\tr3, r6, #3\t\t\t@ SVC mode (an interrupt handler is entered in IRQ mode: &12 -> &13)\n\tmsr\tcpsr_c, r3\n\tmov\tr7, lr\t\t\t\t@ lr_svc: the interrupted code's\n\tmov\tr4, sp\n\tbic\tsp, sp, #7");
      A (&o, "\tbl\t%s\n\tmov\tsp, r4\n\tmov\tlr, r7\n\tmsr\tcpsr_c, r6\t\t\t@ the mode we were called in\n\tmov\tr12, r6\n\tcmp\tr0, #0\n\tldmfd\tsp!, {r0-r11, lr}\n\tldreq\tlr, [sp], #4\t\t\t@ 0: claim the vector: return to the address the kernel stacked\n\tmsr\tcpsr_f, r12\n\tmov\tpc, lr", v->handler);
    }
  A (&o, "%s", "");
  A (&o, "done:\t\t\t\t\t\t@ r0 = 0 (V clear) or an error pointer (V set)\n\tcmp\tr0, #0\n\tldmfdeq\tsp!, {r4-r11, pc}\n\tmov\tr1, #0\n\tcmp\tr1, #0x80000000\n\tldmfd\tsp!, {r4-r11, pc}");
  buf_addc (&o.b, '\n');
  free (ver); free (helpline); free (nums);
  return o.b.s;
}

static char *generate_h (const Module *m, const char *src)
{
  Out o;
  char *ver;
  unsigned vnum = 0;
  int i;
  size_t tl = strlen (m->title);
  Buf guard;
  const char *p;

  ver = strncmp (m->help, m->title, tl) == 0 ? strip (m->help + tl) : xstrdup ("");
  {
    /* re.match (r"(\d+)\.(\d+)", ver): the leading digits, a dot, digits */
    const char *q = ver;
    size_t a = 0, b;
    while (isdigit ((unsigned char) q[a])) a++;
    if (a && q[a] == '.' && isdigit ((unsigned char) q[a + 1]))
      {
        b = 0;
        while (isdigit ((unsigned char) q[a + 1 + b])) b++;
        {
          unsigned major = (unsigned) strtoul (q, NULL, 10), minor = (unsigned) strtoul (q + a + 1, NULL, 10);
          vnum = b == 2 ? major * 100 + minor : major * 100 + minor * 10;
        }
      }
  }
  buf_init (&guard);
  buf_adds (&guard, "_MODKIT_");
  for (p = m->title; *p; p++) buf_addc (&guard, is_word ((unsigned char) *p) ? *p : '_');
  buf_adds (&guard, "_H_");
  out_init (&o);
  A (&o, "/* Generated by mkmodhdr.py from %s.  DO NOT EDIT. */", base_name (src));
  A (&o, "#ifndef %s\n#define %s\n\n#include \"kernel.h\"\n", guard.s, guard.s);
  A (&o, "#define Module_Title\t\t\"%s\"\n#define Module_Help\t\t\"%s\"\n#define Module_VersionString\t\"%s\"\n#define Module_VersionNumber\t%u\n#ifndef Module_Date\n#define Module_Date\t\t\"%s\"\n#endif\n", m->title, m->title, ver, vnum, m->date ? m->date : "");
  A (&o, "#ifdef __cplusplus\nextern \"C\" {\n#endif\n");
  if (m->init) A (&o, "_kernel_oserror *%s (const char *tail, int podule_base, void *pw);", m->init);
  if (m->final) A (&o, "_kernel_oserror *%s (int fatal, int podule_base, void *pw);", m->final);
  if (m->ncmds)
    {
      A (&o, "_kernel_oserror *%s (const char *arg_string, int argc, int number, void *pw);", m->cmd_handler);
      A (&o, "#define help_PRINT_BUFFER\t\t((_kernel_oserror *) arg_string)\n");
      A (&o, "/* Command numbers, as passed to the command handler function */");
      for (i = 0; i < m->ncmds; i++) A (&o, "#undef CMD_%s\n#define CMD_%s (%d)", m->cmds[i].name, m->cmds[i].name, i);
      A (&o, "%s", "");
    }
  if (m->service) A (&o, "void %s (int service_number, _kernel_swi_regs *r, void *pw);", m->service);
  if (m->swi_handler)
    {
      A (&o, "_kernel_oserror *%s (int swi_offset, _kernel_swi_regs *r, void *pw);", m->swi_handler);
      A (&o, "#define Module_SWIChunk\t\t%s", hx (m->swi_chunk));
      for (i = 0; i < m->nswi; i++) A (&o, "#define %s_%s\t\t(%s)", m->swi_prefix, m->swi_names[i], hx (m->swi_chunk + (unsigned) i));
    }
  for (i = 0; i < m->nven; i++) A (&o, "extern void %s (void);\nint %s (_kernel_swi_regs *r, void *pw);", m->ven[i].entry, m->ven[i].handler);
  if (m->nven) A (&o, "\n/* VECTOR_PASSON can be returned from vectors to pass the call on to other claimants; VECTOR_CLAIM to claim it. */\n#define VECTOR_PASSON (1)\n#define VECTOR_CLAIM (0)");
  A (&o, "\n#ifdef __cplusplus\n}\n#endif\n#endif");
  buf_addc (&o.b, '\n');
  free (ver); free (guard.s);
  return o.b.s;
}

/* ---------------------------------------------------------------- the command line */
static void usage (FILE *f)
{
  fputs ("usage: cmunge [-tgcc] [-32bit] [-p|-px] [-D<sym>[=<val>]] [-U<sym>] [-I<dir>] [-throwback] [-s FILE.s] [-d FILE.h] [-o FILE.o] FILE.cmhg\n"
         "  -o FILE  the object   -s FILE  the assembler source   -d FILE  the C header   -p  run the C preprocessor first\n"
         "  accepted without effect: -tgcc -32bit -znoscl -throwback -cmhg.  Refused: -tnorcroft -tlcc -26bit -apcs -zbase -zerrors -zoslib -blank -x<type> -depend\n", f);
}

static const char *argv0dir;

#if !defined (__riscos__)
static int file_exists (const char *p)
{
  FILE *f = fopen (p, "rb");
  if (f) fclose (f);
  return f != NULL;
}
#endif

static char *find_cc (void)
{
  const char *cc = getenv ("CMUNGE_CC");
  if (cc && *cc) return xstrdup (cc);
#if defined (__riscos__)
  return xstrdup ("gcc");
#else
  if (argv0dir && *argv0dir)
    {
      Buf b;
      buf_init (&b);
      buf_printf (&b, "%s/arm-riscos-gnueabihf-gcc", argv0dir);
      if (file_exists (b.s)) return b.s;
      free (b.s);
    }
  return xstrdup ("arm-riscos-gnueabihf-gcc");
#endif
}

/* one argument of a shell command line: quoted for sh on Linux, as it is on RISC OS (file names there have no blanks) */
static void add_arg (Buf *b, const char *a)
{
  buf_addc (b, ' ');
#if defined (__riscos__)
  buf_adds (b, a);
#else
  buf_addc (b, '\'');
  for (; *a; a++)
    {
      if (*a == '\'') buf_adds (b, "'\\''"); else buf_addc (b, *a);
    }
  buf_addc (b, '\'');
#endif
}

static void run (const Buf *cmd, const char *what)
{
  int rc = system (cmd->s);
  if (rc != 0) die ("%s failed", what);
}

static char *temp_name (const char *tag)
{
  const char *dir = getenv ("TMPDIR");
  Buf b;
  static unsigned counter;
  unsigned long pid = (unsigned long) getpid ();
  buf_init (&b);
  buf_printf (&b, "%s/cmunge%lu%s%u", dir && *dir ? dir : ".", pid, tag, counter++);
  return b.s;
}

int main (int argc, char **argv)
{
  const char *out_o = NULL, *out_s = NULL, *out_h = NULL, *infile = NULL;
  int pre = 0, i, ncpp = 0;
  char **cpp = xmalloc (sizeof (char *) * (size_t) (argc + 1));
  char *src, *srctext, *asm_text, *h_text, *ppfile = NULL, *asmfile = NULL, *cc;
  Module m;
  const char *slash;

  progname = "cmunge";
  slash = strrchr (argv[0], '/');
  argv0dir = slash ? xstrndup (argv[0], (size_t) (slash - argv[0])) : "";
#if defined (__linux__)
  {
    /* where this program really is (it may have been found on PATH): the compiler of the tool chain is next to it */
    char exe[PATH_MAX + 1];
    ssize_t n = readlink ("/proc/self/exe", exe, PATH_MAX);
    if (n > 0)
      {
        char *sl;
        exe[n] = 0;
        sl = strrchr (exe, '/');
        if (sl) argv0dir = xstrndup (exe, (size_t) (sl - exe));
      }
  }
#endif
  for (i = 1; i < argc; i++)
    {
      const char *a = argv[i];
      if (!strcmp (a, "-tgcc") || !strcmp (a, "-32bit") || !strcmp (a, "-znoscl") || !strcmp (a, "-throwback") || !strcmp (a, "-cmhg")) continue;
      if (!strcmp (a, "-o") || !strcmp (a, "-s") || !strcmp (a, "-d"))
        {
          if (++i >= argc) die ("%s needs a file name", a);
          if (a[1] == 'o') out_o = argv[i]; else if (a[1] == 's') out_s = argv[i]; else out_h = argv[i];
        }
      else if (!strcmp (a, "-apcs") || !strcmp (a, "-depend")) die ("%s is not supported (modkit has one calling standard and no AMU dependency files)", a);
      else if (!strcmp (a, "-p") || !strcmp (a, "-px")) pre = 1;
      else if ((a[1] == 'D' || a[1] == 'U' || a[1] == 'I') && a[0] == '-' && a[2]) cpp[ncpp++] = (char *) a;
      else if (!strcmp (a, "-D") || !strcmp (a, "-U") || !strcmp (a, "-I"))
        {
          Buf b;
          if (++i >= argc) die ("%s needs an argument", a);
          buf_init (&b); buf_adds (&b, a); buf_adds (&b, argv[i]);
          cpp[ncpp++] = b.s;
        }
      else if (!strcmp (a, "-h")) { usage (stdout); return 0; }
      else if (!strcmp (a, "-tnorcroft")) die ("the Norcroft toolchain is not supported by the modkit version of cmunge");
      else if (!strcmp (a, "-tlcc")) die ("the LCC toolchain is not supported by the modkit version of cmunge");
      else if (!strcmp (a, "-26bit")) die ("26 bit code is not supported by the modkit version of cmunge");
      else if (!strcmp (a, "-zbase")) die ("Image__RO_Base is not supported by the modkit version of cmunge");
      else if (!strcmp (a, "-zerrors")) die ("-zerrors (errors and veneers only) is not supported by the modkit version of cmunge");
      else if (!strcmp (a, "-zoslib")) die ("-zoslib (OSLib types in the header) is not supported by the modkit version of cmunge");
      else if (!strcmp (a, "-blank")) die ("-blank is not supported by the modkit version of cmunge");
      else if (a[0] == '-' && (a[1] == 'x' || a[1] == 't' || a[1] == 'z')) die ("%s is not supported by the modkit version of cmunge", a);
      else if (a[0] == '-') die ("unknown option %s (cmunge -h)", a);
      else if (!infile) infile = a;
      else die ("more than one input file: %s and %s", infile, a);
    }
  if (!infile) die ("no input file (cmunge -h)");
  if (!out_o && !out_s && !out_h) die ("nothing to do: give -o, -s or -d");

  src = (char *) infile;
  if (pre)
    {
      Buf cmd;
      cc = find_cc ();
      ppfile = temp_name ("p");
      buf_init (&cmd);
      buf_adds (&cmd, cc);
      add_arg (&cmd, "-E"); add_arg (&cmd, "-P"); add_arg (&cmd, "-x"); add_arg (&cmd, "assembler-with-cpp");
      for (i = 0; i < ncpp; i++) add_arg (&cmd, cpp[i]);
      add_arg (&cmd, "-o"); add_arg (&cmd, ppfile); add_arg (&cmd, infile);
      run (&cmd, "the preprocessor");
      src = ppfile;
    }
  srcname = infile;
  srctext = (char *) read_file (src, NULL);
  parse_cmhg (srctext, &m);
  asm_text = generate_asm (&m, infile);
  h_text = generate_h (&m, infile);
  if (out_h) write_file (out_h, h_text, strlen (h_text));
  asmfile = (char *) out_s;
  if (!asmfile && out_o) asmfile = temp_name ("s");
  if (asmfile) write_file (asmfile, asm_text, strlen (asm_text));
  if (out_o)
    {
      Buf cmd;
      cc = find_cc ();
      buf_init (&cmd);
      buf_adds (&cmd, cc);
      add_arg (&cmd, "-march=armv6"); add_arg (&cmd, "-x"); add_arg (&cmd, "assembler"); add_arg (&cmd, "-c"); add_arg (&cmd, asmfile); add_arg (&cmd, "-o"); add_arg (&cmd, out_o);
      run (&cmd, "the assembler");
    }
  if (ppfile) remove (ppfile);
  if (asmfile && asmfile != out_s) remove (asmfile);
  return 0;
}
