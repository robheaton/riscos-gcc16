/* cmunge64 - CMHG file -> the header of a 64-bit module (AArch64 assembler) and the C header, for the modules of the kit (aarch64-riscos-gcc -mmodule): the C version of riscos64/kit/cmunge64.py
   (the same output, byte for byte), for hosts without Python (32 bit RISC OS).  The parser and the C header are those of modkit/src/cmunge.c (which is the C version of mkmodhdr.py); the assembler
   text is generated for AArch64 as cmunge64.py does.

     cmunge64 [-p] [-D<sym>[=<val>]] [-U<sym>] [-I<dir>] [-s FILE.S] [-d FILE.h] [-o FILE.o] FILE.cmhg

   -p      run the C preprocessor over the file first (-D, -U and -I go to it)
   -o FILE the object: the header assembled by CMUNGE64_CC (default: aarch64-riscos-gcc next to this program, or on the search path), with -mmodule
   Supported and refused CMHG items: see cmunge64.py (the veneers, pdriver-handler and module-is-runnable are refused with a message). */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
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
  unsigned min, max, gstrans;
  int fs_command, status_cmd, international, add_syntax;      /* the flags of the information word: bits 31, 30 (status / configure), 28; add-syntax joins the two texts */
  unsigned char *help; size_t helplen; int has_help;
  unsigned char *syntax; size_t synlen; int has_syntax;
  char *handler; int nohandler;                                                /* handler: FN (a function of its own), no-handler: (no code) */
} Cmd;

typedef struct { char *kind, *entry, *handler; unsigned *ev; int nev; int pw, carry, errc; } Veneer;      /* ev: the event numbers that an event-handler accepts */

typedef struct
{
  char *title, *help, *date, *init, *final, *service;
  unsigned *svc; int nsvc;
  Cmd *cmds; int ncmds; char *cmd_handler;
  unsigned swi_chunk; char *swi_prefix; char **swi_names; char **swi_handlers; int nswi; char *swi_handler;
  char *swi_dec, *swi_dec2;                                 /* swi-decoding-code: FN, or NAME/NUMBER (the name to number and the number to name functions) */
  int swi_notable;                                          /* swi-decoding-code without a swi-decoding-table: the header has no table, the kernel asks the code in both directions */
  int base;                                                 /* -zbase: Image__RO_Base, a word that holds the address of the module */
  int notreent;                                             /* module-is-not-reentrant: (accepted, no effect) */
  Veneer *ven; int nven;
  char *pd_entry; char **pd_names; int npd;                 /* pdriver-handler: ENTRY, FN, FN ... */
  int runnable;
  int cxx;                                                  /* module-is-c-plus-plus: */
  unsigned char *mfile; size_t mfilelen;                    /* international-help-file: the name of the Messages file */
  unsigned error_base;                                      /* error-chunk-base-number: (alias error-base:): ERROR_BASE, where the numbers of error-identifiers start */
  struct { char *name; unsigned num; unsigned char *msg; size_t len; } *errs; int nerr;       /* error-identifiers: NAME ([NUMBER,] "message") ... (a number of 0 is 'the next one') */
} Module;

static const char *srcname;                  /* the CMHG file, for messages */
static int g_zerrors;                                       /* -zerrors: only error-chunk-base-number: and error-identifiers: are accepted, and only the error blocks are made */
static void parse_error_identifiers (Module *m, const char *r);

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

/* A number of a CMHG file after the C preprocessor (-p): a constant expression.  The OS's own headers turn the names of services and SWIs into "(0x60)" (Hdr2H), so a number may be in
   parentheses and may be a small expression: decimal, 0xHEX or &HEX numbers (a & that starts a term is the hex prefix, one after a term is "and"), unary - + ~, * / %, + -, << >>, & ^ |
   (C's order of precedence; a number may end in u or l). */
typedef struct { const char *p; const char *all; } Ex;
static long long ex_or (Ex *x);
static void ex_skip (Ex *x) { while (*x->p == ' ' || *x->p == '\t') x->p++; }
static void ex_fail (Ex *x) { cmhg_error ("%s is not a number", x->all); }
static long long ex_atom (Ex *x)
{
  long long v;
  char *end;
  ex_skip (x);
  if (*x->p == '(')
    {
      x->p++;
      v = ex_or (x);
      ex_skip (x);
      if (*x->p != ')') ex_fail (x);
      x->p++;
      return v;
    }
  if (*x->p == '-') { x->p++; return -ex_atom (x); }
  if (*x->p == '+') { x->p++; return ex_atom (x); }
  if (*x->p == '~') { x->p++; return ~ex_atom (x); }
  if (*x->p == '&') v = (long long) strtoull (x->p + 1, &end, 16), x->p += 1;
  else if (x->p[0] == '0' && (x->p[1] == 'x' || x->p[1] == 'X')) v = (long long) strtoull (x->p + 2, &end, 16), x->p += 2;
  else v = (long long) strtoull (x->p, &end, 10);
  if (end == x->p) ex_fail (x);
  x->p = end;
  while (*x->p == 'u' || *x->p == 'U' || *x->p == 'l' || *x->p == 'L') x->p++;
  return v;
}
static long long ex_mul (Ex *x)
{
  long long v = ex_atom (x);
  for (;;)
    {
      ex_skip (x);
      if (*x->p == '*') { x->p++; v *= ex_atom (x); }
      else if (*x->p == '/' || *x->p == '%')
        {
          char op = *x->p++;
          long long d = ex_atom (x);
          if (d == 0) cmhg_error ("%s: division by zero", x->all);
          v = op == '/' ? v / d : v % d;
        }
      else return v;
    }
}
static long long ex_add (Ex *x)
{
  long long v = ex_mul (x);
  for (;;)
    {
      ex_skip (x);
      if (*x->p == '+') { x->p++; v += ex_mul (x); }
      else if (*x->p == '-') { x->p++; v -= ex_mul (x); }
      else return v;
    }
}
static long long ex_shift (Ex *x)
{
  long long v = ex_add (x);
  for (;;)
    {
      ex_skip (x);
      if (x->p[0] == '<' && x->p[1] == '<') { long long n; x->p += 2; n = ex_add (x); v = n >= 0 && n < 64 ? (long long) ((unsigned long long) v << n) : 0; }
      else if (x->p[0] == '>' && x->p[1] == '>') { long long n; x->p += 2; n = ex_add (x); v = n >= 0 && n < 64 ? (long long) ((unsigned long long) v >> n) : 0; }
      else return v;
    }
}
static long long ex_and (Ex *x)
{
  long long v = ex_shift (x);
  for (;;)
    {
      ex_skip (x);
      if (*x->p == '&') { x->p++; v &= ex_shift (x); }
      else return v;
    }
}
static long long ex_xor (Ex *x)
{
  long long v = ex_and (x);
  for (;;)
    {
      ex_skip (x);
      if (*x->p == '^') { x->p++; v ^= ex_and (x); }
      else return v;
    }
}
static long long ex_or (Ex *x)
{
  long long v = ex_xor (x);
  for (;;)
    {
      ex_skip (x);
      if (*x->p == '|') { x->p++; v |= ex_xor (x); }
      else return v;
    }
}

static unsigned parse_int (const char *s)
{
  Ex x;
  long long v;
  x.p = x.all = s;
  v = ex_or (&x);
  ex_skip (&x);
  if (*x.p) ex_fail (&x);
  if (v < 0 || v > 0xFFFFFFFFLL) cmhg_error ("%s is out of range", s);
  return (unsigned) v;
}

/* ---------------------------------------------------------------- the CMHG parser */
typedef struct { char *key; char *rest; } Line;

/* a ';' that is not inside a string starts a comment that runs to the end of the line (the line end stays) */
static char *strip_comments (const char *text)
{
  Buf o;
  const char *p = text;
  int inq = 0;
  buf_init (&o);
  while (*p)
    {
      char c = *p;
      if (inq)
        {
          buf_addc (&o, c);
          if (c == '\\' && p[1]) { buf_addc (&o, p[1]); p += 2; continue; }
          if (c == '"' || c == '\n') inq = 0;
        }
      else if (c == '"') { inq = 1; buf_addc (&o, c); }
      else if (c == ';')
        {
          while (*p && *p != '\n') p++;
          continue;
        }
      else buf_addc (&o, c);
      p++;
    }
  return o.s;
}

/* the depth of the parentheses that are open at the end of S (not counting the ones inside strings) and its last character that is not white space */
static void depth_and_last (const char *s, int *depth, int *last)
{
  int inq = 0;
  *depth = 0; *last = 0;
  while (*s)
    {
      char c = *s;
      if (inq)
        {
          if (c == '\\') { s++; if (*s) s++; continue; }
          if (c == '"') inq = 0;
        }
      else if (c == '"') { inq = 1; *last = c; }
      else
        {
          if (c == '(') (*depth)++; else if (c == ')') (*depth)--;
          if (!is_space ((unsigned char) c)) *last = c;
        }
      s++;
    }
}

/* the text is the name of a handler and nothing else: [ws] name [ws] */
static int handler_only (const char *s)
{
  while (is_space ((unsigned char) *s)) s++;
  if (*s == '-')                                            /* "-": the table has no handler of its own */
    {
      s++;
      while (is_space ((unsigned char) *s)) s++;
      return *s == 0;
    }
  if (!(is_alpha ((unsigned char) *s) || *s == '_')) return 0;
  while (is_word ((unsigned char) *s)) s++;
  while (is_space ((unsigned char) *s)) s++;
  return *s == 0;
}

/* the line starts with  key:  (a letter, letters, digits, _ and -, blanks, a colon) */
static int directive_line (const char *s)
{
  if (!is_alpha ((unsigned char) *s)) return 0;
  s++;
  while (is_word ((unsigned char) *s) || *s == '-') s++;
  while (is_space ((unsigned char) *s)) s++;
  return *s == ':';
}

/* [key (lower case), the text after the colon]: a line goes on in the next one when the next one starts with white space, when it ends in a comma or has a parenthesis open, and - for the command table -
   when it holds the name of the handler and nothing else (the entries then follow, at the start of the line or not) */
static Line *logical_lines (const char *text, int *nlines)
{
  char *clean = strip_comments (text);
  Line *out = NULL;
  int n = 0;
  Buf *bufs = NULL;
  const char *p = clean;
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
      if (*l)
        {
          int more = 0;
          if (n > 0)
            {
              int depth, last;
              depth_and_last (bufs[n - 1].s, &depth, &last);
              more = depth > 0 || last == ',' || line[0] == ' ' || line[0] == '\t';
              if (!more && strcmp (out[n - 1].key, "command-keyword-table") == 0 && handler_only (bufs[n - 1].s) && !directive_line (line)) more = 1;
              if (more)
                {
                  buf_addc (&bufs[n - 1], '\n');
                  buf_adds (&bufs[n - 1], line);
                }
            }
          if (!more)
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
            }
        }
      free (line);
      if (!nl) break;
      p = nl + 1;
    }
  { int i; for (i = 0; i < n; i++) out[i].rest = bufs[i].s; }
  free (bufs);
  free (clean);
  *nlines = n;
  return out;
}

/* the CMHG of the RISC OS build has names in quotes (a macro gives "RTC"): the quotes of the first word, or of the whole value, are taken away */
static char *unquote_first (const char *s)
{
  char *t = strip (s);
  if (t[0] == '"')
    {
      char *j = strchr (t + 1, '"');
      if (j)
        {
          Buf b;
          buf_init (&b);
          buf_addn (&b, t + 1, (size_t) (j - t - 1));
          buf_adds (&b, j + 1);
          free (t);
          return b.s;
        }
    }
  return t;
}
/* Python's  s.strip ('"') */
static char *strip_quotes (char *s)
{
  size_t a = 0, b = strlen (s);
  while (a < b && s[a] == '"') a++;
  while (b > a && s[b - 1] == '"') b--;
  return xstrndup (s + a, b - a);
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
  if (rest[k] == '-' && !(is_word ((unsigned char) rest[k + 1]) || rest[k + 1] == '$' || rest[k + 1] == '%'))
    {
      m->cmd_handler = NULL;                                  /* "-": every command has a function of its own, or none */
      k++;
    }
  else
    {
      if (!(is_alpha ((unsigned char) rest[k]) || rest[k] == '_')) cmhg_error ("command-keyword-table: needs the name of the handler function (or - when every command has its own handler: or none)");
      while (is_word ((unsigned char) rest[k])) k++;
      m->cmd_handler = xstrndup (rest + pos, k - pos);
    }
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
      c->min = 0; c->max = 0;
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
                  if (rest[e] == '(')                                  /* a number that the preprocessor made, in parentheses: (2) */
                    {
                      int depth = 0;
                      while (e < len && (depth > 0 || e == pos))
                        {
                          if (rest[e] == '(') depth++;
                          else if (rest[e] == ')') depth--;
                          e++;
                        }
                      if (depth != 0) cmhg_error ("a number was expected near '%.30s'", rest + pos);
                    }
                  else if (rest[e] == '&' || (rest[e] == '0' && (rest[e + 1] == 'x' || rest[e + 1] == 'X')))
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
              else if (strcmp (key, "international") == 0) c->international = 1;
              else if (strcmp (key, "add-syntax") == 0) c->add_syntax = 1;
              else if (strcmp (key, "configure") == 0 || strcmp (key, "status") == 0) c->status_cmd = 1;
              else if (strcmp (key, "fs-command") == 0) c->fs_command = 1;
              else if (strcmp (key, "no-handler") == 0) c->nohandler = 1;
              else if (strcmp (key, "handler") == 0)
                {
                  size_t e = pos;
                  if (!(is_alpha ((unsigned char) rest[e]) || rest[e] == '_')) cmhg_error ("handler: needs the name of a function in command %s", c->name);
                  if (c->handler) cmhg_error ("Only supply one handler: field in command %s", c->name);
                  while (is_word ((unsigned char) rest[e])) e++;
                  c->handler = xstrndup (rest + pos, e - pos);
                  pos = e;
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
          if (c->min > 255) cmhg_error ("min-args: must be between 0 and 255 in command %s", c->name);
          if (c->max > 255) cmhg_error ("max-args: must be between 0 and 255 in command %s", c->name);
          if (c->gstrans > 255) cmhg_error ("gstrans-map: may only describe 8 bits in command %s", c->name);
          if (c->add_syntax && c->international) cmhg_error ("add-syntax: and international: are mutually exclusive in command %s", c->name);
        }
      if (c->nohandler && c->handler) cmhg_error ("no-handler: and handler: are mutually exclusive in command %s", c->name);
      if (!m->cmd_handler && !c->handler) c->nohandler = 1;          /* (CMunge: a command with nothing to call has no handler) */
    }
  if (!n) cmhg_error ("the command-keyword-table has no commands");
  m->ncmds = n;
}

/* the value of a directive that names one function: a C identifier (CMHG's own options in brackets, such as swi-handler-code: name (flags-capable:), are not supported) */
static char *one_name (const char *key, char *r)
{
  const char *p = r;
  if (!is_alpha ((unsigned char) *p) && *p != '_') cmhg_error ("%s: needs the name of a function", key);
  while (is_word ((unsigned char) *p)) p++;
  if (*p) cmhg_error ("%s: needs one function name; '%s' is not supported", key, p);
  return r;
}

static Veneer *add_veneer (Module *m, const char *kind, const char *entry, const char *handler)
{
  Veneer *v;
  m->ven = xrealloc (m->ven, sizeof (Veneer) * (size_t) (m->nven + 1));
  v = &m->ven[m->nven++];
  memset (v, 0, sizeof *v);
  v->kind = xstrdup (kind);
  v->entry = xstrdup (entry);
  v->handler = xstrdup (handler);
  v->pw = 12;
  return v;
}

/* irq-handlers: / vector-handlers: / generic-veneers:   ENTRY[/HANDLER][(options)] ...   The options that CMunge allows: private-word: rN and carry-capable: for generic-veneers, error-capable: for vector-handlers, none for irq-handlers */
static void parse_handlers (Module *m, const char *key, const char *r)
{
  int allow_pw = strcmp (key, "generic-veneers") == 0, allow_carry = allow_pw, allow_err = strcmp (key, "vector-handlers") == 0;
  size_t pos = 0, len = strlen (r);
  for (;;)
    {
      size_t k, e;
      char *entry, *handler;
      Veneer *v;
      int have_pw = 0, have_carry = 0, have_err = 0;
      while (pos < len && (is_space ((unsigned char) r[pos]) || r[pos] == ',')) pos++;
      if (pos >= len) break;
      k = pos;
      if (!(is_alpha ((unsigned char) r[k]) || r[k] == '_')) cmhg_error ("%s: a function name was expected near '%.30s'", key, r + pos);
      while (is_word ((unsigned char) r[k])) k++;
      entry = xstrndup (r + pos, k - pos);
      pos = k;
      if (r[pos] == '/' && (is_alpha ((unsigned char) r[pos + 1]) || r[pos + 1] == '_'))
        {
          e = pos + 1;
          while (is_word ((unsigned char) r[e])) e++;
          handler = xstrndup (r + pos + 1, e - pos - 1);
          pos = e;
        }
      else
        {
          Buf hb;                                                     /* CMunge: the handler of NAME is NAME_handler */
          buf_init (&hb);
          buf_adds (&hb, entry);
          buf_adds (&hb, "_handler");
          handler = hb.s;
        }
      while (pos < len && is_space ((unsigned char) r[pos])) pos++;
      v = add_veneer (m, key, entry, handler);
      free (entry); free (handler);
      if (pos < len && r[pos] == '(')
        {
          pos++;
          for (;;)
            {
              const char *opt;
              while (pos < len && (is_space ((unsigned char) r[pos]) || r[pos] == ',')) pos++;
              if (pos >= len) cmhg_error ("%s: Ran out of file when parsing handler details!", key);
              if (r[pos] == ')') { pos++; break; }
              if (strncmp (r + pos, "private-word:", 13) == 0) opt = "private-word", pos += 13;
              else if (strncmp (r + pos, "carry-capable:", 14) == 0) opt = "carry-capable", pos += 14;
              else if (strncmp (r + pos, "error-capable:", 14) == 0) opt = "error-capable", pos += 14;
              else cmhg_error ("%s: Unknown argument in handler: %.30s", key, r + pos);
              while (pos < len && is_space ((unsigned char) r[pos])) pos++;
              if (strcmp (opt, "private-word") == 0 ? !allow_pw : strcmp (opt, "carry-capable") == 0 ? !allow_carry : !allow_err) cmhg_error ("%s: %s argument not permitted", key, opt);
              if (strcmp (opt, "private-word") == 0 ? have_pw : strcmp (opt, "carry-capable") == 0 ? have_carry : have_err) cmhg_error ("%s: %s supplied twice!", key, opt);
              if (strcmp (opt, "private-word") == 0)
                {
                  unsigned n = 0;
                  if (!((r[pos] == 'r' || r[pos] == 'R') && isdigit ((unsigned char) r[pos + 1]))) cmhg_error ("%s: private-word value must be a register!", key);
                  pos++;
                  while (isdigit ((unsigned char) r[pos])) { if (n < 1000) n = n * 10 + (unsigned) (r[pos] - '0'); pos++; }
                  if (n > 12) cmhg_error ("%s: private-word register must be r0-r12!", key);
                  v->pw = (int) n; have_pw = 1;
                }
              else if (strcmp (opt, "carry-capable") == 0) { v->carry = 1; have_carry = 1; }
              else { v->errc = 1; have_err = 1; }
            }
          while (pos < len && is_space ((unsigned char) r[pos])) pos++;
        }
    }
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
      if (g_zerrors && strcmp (key, "error-chunk-base-number") != 0 && strcmp (key, "error-base") != 0 && strcmp (key, "error-identifiers") != 0)
        cmhg_error ("%s is not supported by cmunge in 'errors' mode (-zerrors)", key);
      if (strcmp (key, "title-string") == 0) m->title = unquote_first (r);
      else if (strcmp (key, "help-string") == 0) m->help = unquote_first (r);
      else if (strcmp (key, "date-string") == 0) m->date = unquote_first (r);
      else if (strcmp (key, "initialisation-code") == 0) m->init = one_name (key, r);
      else if (strcmp (key, "finalisation-code") == 0) m->final = one_name (key, r);
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
      else if (strcmp (key, "swi-chunk-base-number") == 0)
        {
          m->swi_chunk = parse_int (r);
          if (m->swi_chunk == 0 || (m->swi_chunk & 0x3f)) cmhg_error ("swi-chunk-base-number: 0x%08x is not a SWI chunk (a multiple of 64, not 0)", m->swi_chunk);
          if (m->swi_chunk & 0x20000) cmhg_error ("swi-chunk-base-number: 0x%08x has the X bit set (&20000)", m->swi_chunk);
        }
      else if (strcmp (key, "swi-decoding-table") == 0)
        {
          int np, k;
          char **w = split_words (r, 1, &np);
          if (np < 1) cmhg_error ("swi-decoding-table: needs the prefix");
          m->swi_prefix = strip_quotes (w[0]);
          m->swi_names = np > 1 ? xmalloc (sizeof (char *) * (size_t) (np - 1)) : NULL;
          m->swi_handlers = np > 1 ? xmalloc (sizeof (char *) * (size_t) (np - 1)) : NULL;
          if (strchr (r, '(')) cmhg_error ("swi-decoding-table: SWI handlers cannot be passed parameters");
          for (k = 1; k < np; k++)
            {
              char *nm = strip_quotes (w[k]), *slash = strchr (nm, '/');
              m->swi_handlers[k - 1] = NULL;
              if (slash)
                {
                  char *q;
                  *slash = 0;
                  q = slash + 1;
                  if (!(is_alpha ((unsigned char) *q) || *q == '_')) cmhg_error ("swi-decoding-table: '%s' is not a function name", q);
                  for (; *q; q++) if (!is_word ((unsigned char) *q)) cmhg_error ("swi-decoding-table: '%s' is not a function name", slash + 1);
                  m->swi_handlers[k - 1] = xstrdup (slash + 1);
                }
              m->swi_names[k - 1] = nm;
            }
          m->nswi = np - 1;
        }
      else if (strcmp (key, "swi-handler-code") == 0) m->swi_handler = one_name (key, r);
      else if (strcmp (key, "swi-decoding-code") == 0)
        {
          int np;
          char **w;
          char *slash, *q;
          if (m->swi_dec) cmhg_error ("Only supply one swi-decoding-code!");
          w = split_words (r, 1, &np);                                    /* FN, or NAME/NUMBER: the name to number and the number to name functions */
          if (np != 1) cmhg_error ("swi-decoding-code: needs one function name, or NAME/HANDLER (the name to number function and the number to name function)");
          slash = strchr (w[0], '/');
          if (slash) { *slash = 0; q = slash + 1; }
          else q = NULL;
          if (!(is_alpha ((unsigned char) w[0][0]) || w[0][0] == '_')) cmhg_error ("swi-decoding-code: needs one function name, or NAME/HANDLER (the name to number function and the number to name function)");
          { const char *c; for (c = w[0]; *c; c++) if (!is_word ((unsigned char) *c)) cmhg_error ("swi-decoding-code: needs one function name, or NAME/HANDLER (the name to number function and the number to name function)"); }
          if (q)
            {
              const char *c;
              if (!(is_alpha ((unsigned char) *q) || *q == '_')) cmhg_error ("swi-decoding-code: needs one function name, or NAME/HANDLER (the name to number function and the number to name function)");
              for (c = q; *c; c++) if (!is_word ((unsigned char) *c)) cmhg_error ("swi-decoding-code: needs one function name, or NAME/HANDLER (the name to number function and the number to name function)");
            }
          m->swi_dec = w[0];
          m->swi_dec2 = q ? xstrdup (q) : NULL;
        }
      else if (strcmp (key, "error-chunk-base-number") == 0 || strcmp (key, "error-base") == 0) m->error_base = parse_int (r);
      else if (strcmp (key, "error-identifiers") == 0) parse_error_identifiers (m, r);
      else if (strcmp (key, "module-is-not-reentrant") == 0)
        {
          if (*r) cmhg_error ("module-is-not-reentrant: takes no value");
          m->notreent = 1;
        }
      else if (strcmp (key, "irq-handlers") == 0 || strcmp (key, "vector-handlers") == 0 || strcmp (key, "generic-veneers") == 0 || strcmp (key, "vector-traps") == 0)
        {
          parse_handlers (m, key, r);
        }
      else if (strcmp (key, "pdriver-handler") == 0)
        {
          /* ENTRY, FUNCTION, FUNCTION ...: ENTRY is the symbol whose value is r0 of PDriver_DeclareDriver (not a C function); the functions are those for reason codes 0, 1, 2 ... (CMunge: read_pdriver) */
          char *copy, *q, *e;
          int k, ncomma = 0;
          if (m->pd_entry) cmhg_error ("Only supply one pdriver-handler!");
          copy = xstrdup (r);
          q = copy;
          while (is_space ((unsigned char) *q)) q++;
          e = q + strlen (q);
          while (e > q && is_space ((unsigned char) e[-1])) e--;
          *e = 0;
          for (k = 0; q[k]; k++) if (q[k] == ',') ncomma++;
          {
            char *start = q;
            int word = 0;
            for (;;)
              {
                char *comma = strchr (start, ','), *a = start, *b = comma ? comma : start + strlen (start);
                char *w;
                size_t j;
                while (a < b && is_space ((unsigned char) *a)) a++;
                while (b > a && is_space ((unsigned char) b[-1])) b--;
                w = xstrndup (a, (size_t) (b - a));
                if (word == 0 && (!(is_alpha ((unsigned char) w[0]) || w[0] == '_') || strlen (w) == 0)) cmhg_error ("No entry point defined for pdriver-handler!");
                for (j = 0; w[j]; j++) if (!is_word ((unsigned char) w[j]) || (j == 0 && isdigit ((unsigned char) w[j]))) { if (word == 0) cmhg_error ("No entry point defined for pdriver-handler!"); else cmhg_error ("Wrong separator found in pdriver-handler!"); }
                if (word == 0) { if (ncomma == 0) cmhg_error ("Wrong separator found in pdriver-handler!"); m->pd_entry = w; }
                else if (w[0]) { m->pd_names = xrealloc (m->pd_names, sizeof m->pd_names[0] * (size_t) (m->npd + 1)); m->pd_names[m->npd++] = w; }
                else free (w);
                word++;
                if (!comma) break;
                start = comma + 1;
              }
          }
          free (copy);
        }
      else if (strcmp (key, "event-handler") == 0)
        {
          /* ENTRY[/HANDLER] [number ...]: a veneer for the event vector; it passes on every event that is not in the list (CMunge: "fast accept/reject code") and otherwise works as a vector-handlers veneer */
          int np, k;
          char **w = split_words (r, 1, &np);
          char *slash;
          Veneer *v;
          if (np < 1) cmhg_error ("event-handler: needs the name of the handler function");
          {                                                               /* (the event numbers may be in parentheses: the preprocessor made them) */
            const char *c;
            for (c = r; *c; c++)
              if (*c == '(')
                {
                  const char *d = c + 1;
                  while (is_space ((unsigned char) *d)) d++;
                  if (isalpha ((unsigned char) *d)) cmhg_error ("Event handlers cannot be passed parameters");
                }
          }
          slash = strchr (w[0], '/');
          if (slash)
            {
              *slash = 0;
              v = add_veneer (m, key, w[0], slash + 1);
            }
          else
            {
              Buf hb;
              buf_init (&hb);
              buf_adds (&hb, w[0]);
              buf_adds (&hb, "_handler");
              v = add_veneer (m, key, w[0], hb.s);
              free (hb.s);
            }
          for (k = 1; k < np; k++)
            {
              v->ev = xrealloc (v->ev, sizeof (unsigned) * (size_t) (v->nev + 1));
              v->ev[v->nev++] = parse_int (w[k]);
            }
        }
      else if (strcmp (key, "module-is-runnable") == 0)
        {
          if (*r) cmhg_error ("module-is-runnable: takes no value here (CMunge's simple-app: is not supported)");
          m->runnable = 1;
        }
      else if (strcmp (key, "module-is-c-plus-plus") == 0)
        {
          if (*r) cmhg_error ("module-is-c-plus-plus: takes no value");
          m->cxx = 1;
        }
      else if (strcmp (key, "international-help-file") == 0)
        {
          size_t pos = 0;
          m->mfile = parse_string_literals (lines[i].rest, &pos, &m->mfilelen);
        }
      else if (strcmp (key, "library-enter-code") == 0 || strcmp (key, "library-initialisation-code") == 0)
        cmhg_error ("%s: is not supported (it redirects the start-up of the Shared C Library, which a modkit module does not have)", key);
      else cmhg_error ("%s: is not supported", key);
    }
  if (!g_zerrors && (!m->title || !*m->title)) cmhg_error ("title-string: is missing");
  if (!g_zerrors && (!m->help || !*m->help)) cmhg_error ("help-string: is missing");
  if (m->nerr && !m->error_base) cmhg_error ("error-identifiers: errors are defined, but there is no error-chunk-base-number:");
  /* the SWIs of a module: a chunk, a handler and a decoding table go together (CMunge: a prefix of the module's title when there is no table) */
  {
    int per_swi = 0, k;
    for (k = 0; k < m->nswi; k++) if (m->swi_handlers[k]) per_swi = 1;
    if ((m->swi_handler || per_swi) && !m->swi_chunk) cmhg_error ("swi-handler-code: needs a swi-chunk-base-number:");
    if (m->swi_chunk && !m->swi_handler && !per_swi) cmhg_error ("swi-chunk-base-number: needs a swi-handler-code:");
    if (m->swi_prefix && !m->swi_chunk) cmhg_error ("swi-decoding-table: needs a swi-chunk-base-number:");
    if (m->swi_dec && !m->swi_chunk) cmhg_error ("swi-decoding-code: needs a swi-chunk-base-number:");
    m->swi_notable = m->swi_dec != NULL && m->swi_prefix == NULL;
    if ((m->swi_handler || per_swi) && !m->swi_prefix) m->swi_prefix = xstrdup (m->title);
  }
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

/* error-identifiers: NAME ([NUMBER,] "message"), ...  (CMunge) */
static void parse_error_identifiers (Module *m, const char *r)
{
  size_t pos = 0, len = strlen (r);
  for (;;)
    {
      size_t a, b;
      char *name, *numtext = NULL;
      unsigned num = 0;
      unsigned char *msg;
      size_t mlen;
      int i;
      while (pos < len && (r[pos] == ' ' || r[pos] == '\t' || r[pos] == '\n' || r[pos] == '\r' || r[pos] == ',')) pos++;
      if (pos >= len) break;
      a = pos;
      if (!(isalpha ((unsigned char) r[pos]) || r[pos] == '_')) cmhg_error ("error-identifiers: a name and ( was expected near '%.20s'", r + pos);
      while (pos < len && (isalnum ((unsigned char) r[pos]) || r[pos] == '_')) pos++;
      b = pos;
      name = xstrndup (r + a, b - a);
      while (pos < len && isspace ((unsigned char) r[pos])) pos++;
      if (pos >= len || r[pos] != '(') cmhg_error ("error-identifiers: a name and ( was expected near '%.20s'", r + a);
      pos++;
      while (pos < len && isspace ((unsigned char) r[pos])) pos++;
      if (pos < len && r[pos] != '"')                                    /* a number, then a comma */
        {
          const char *c = strchr (r + pos, ',');
          if (!c) cmhg_error ("error-identifiers: %s: a number must be followed by a comma and the message", name);
          numtext = xstrndup (r + pos, (size_t) (c - (r + pos)));
          num = parse_int (numtext);
          free (numtext);
          pos = (size_t) (c - r) + 1;
        }
      msg = parse_string_literals (r, &pos, &mlen);
      while (pos < len && isspace ((unsigned char) r[pos])) pos++;
      if (pos >= len || r[pos] != ')') cmhg_error ("error-identifiers: %s: ) was expected after the message", name);
      pos++;
      for (i = 0; i < m->nerr; i++) if (strcmp (m->errs[i].name, name) == 0) cmhg_error ("error-identifiers: %s is there twice", name);
      if (memchr (msg, 0, mlen)) cmhg_error ("error-identifiers: %s: the message has a zero byte", name);
      m->errs = xrealloc (m->errs, sizeof m->errs[0] * (size_t) (m->nerr + 1));
      m->errs[m->nerr].name = name; m->errs[m->nerr].num = num; m->errs[m->nerr].msg = msg; m->errs[m->nerr].len = mlen;
      m->nerr++;
    }
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

/* CMunge's DateStamp: the help-string is a name (the words up to the first one that starts with a digit), a version (digits, a dot, digits and what follows up to a blank) and a rest.  The version number
   is the digits of the version before and after the dot read as one number (1.23 is 123, 1.5 is 15). */
typedef struct { char *name, *version, *rest; unsigned vnum; } Help;
static void split_help (const char *help_string, Help *h)
{
  const char *s = help_string;
  size_t n, pos = 0, j, i;
  Buf name, digits;
  while (*s && is_space ((unsigned char) *s)) s++;
  n = strlen (s);
  buf_init (&name); buf_init (&digits);
  for (;;)
    {
      int more;
      j = pos;
      while (j < n && !is_space ((unsigned char) s[j])) j++;
      buf_addn (&name, s + pos, j - pos);
      pos = j;
      while (pos < n && is_space ((unsigned char) s[pos])) pos++;
      more = pos < n && !(s[pos] >= '0' && s[pos] <= '9');
      if (more) buf_addc (&name, ' '); else break;
    }
  if (pos >= n || !(s[pos] >= '0' && s[pos] <= '9')) cmhg_error ("Malformed help-string found: %s", help_string);
  j = pos;
  while (j < n && s[j] >= '0' && s[j] <= '9') buf_addc (&digits, s[j++]);
  if (j >= n || s[j] != '.') cmhg_error ("Malformed help-string found: %s", help_string);
  j++;
  while (j < n && s[j] >= '0' && s[j] <= '9') buf_addc (&digits, s[j++]);
  while (j < n && !is_space ((unsigned char) s[j])) j++;
  h->version = xstrndup (s + pos, j - pos);
  pos = j;
  while (pos < n && is_space ((unsigned char) s[pos])) pos++;
  h->rest = xstrdup (s + pos);
  h->name = name.s;
  h->vnum = 0;
  for (i = 0; i < digits.len; i++) h->vnum = h->vnum * 10 + (unsigned) (digits.s[i] - '0');
  free (digits.s);
}

/* the help line of the module: the name, one or two tabs (to column 16), the version, the date in brackets, the rest; underscores are blanks */
static char *help_line (const Module *m)
{
  Help h;
  Buf b;
  char *c;
  split_help (m->help, &h);
  buf_init (&b);
  buf_adds (&b, h.name);
  buf_adds (&b, ((strlen (h.name) + 8) & ~(size_t) 7) >= 16 ? "\t" : "\t\t");
  buf_adds (&b, h.version);
  if (m->date && *m->date) { buf_adds (&b, " ("); buf_adds (&b, m->date); buf_adds (&b, ")"); }
  if (*h.rest) { buf_addc (&b, ' '); buf_adds (&b, h.rest); }
  for (c = b.s; *c; c++) if (*c == '_') *c = ' ';
  free (h.name); free (h.version); free (h.rest);
  return b.s;
}

/* the part of I that one ARM immediate (8 bits at an even position) can hold, from the lowest set bit up: CMunge's representable () */
static unsigned representable (unsigned i)
{
  unsigned mask = 255;
  while (((i & mask) & ~(mask << 2)) == 0)
    {
      mask = (mask << 2) | (mask >> 30);
      if (mask == 255) break;                                              /* I is 0: every window is empty */
    }
  return i & mask;
}

/* swi-decoding-table: Name/function - is there a function of its own for one of the SWIs? */
static int any_swi_function (const Module *m)
{
  int i;
  for (i = 0; i < m->nswi; i++) if (m->swi_handlers[i]) return 1;
  return 0;
}

static int cmp_unsigned (const void *a, const void *b)
{
  unsigned x = *(const unsigned *) a, y = *(const unsigned *) b;
  return x < y ? -1 : x > y;
}

/* ---------------------------------------------------------------- the generator for AArch64: the C version of generate_asm of cmunge64.py (the same text, byte for byte) */
static int cmp_unsigned64 (const void *a, const void *b)
{
  unsigned x = *(const unsigned *) a, y = *(const unsigned *) b;
  return x < y ? -1 : x > y;
}

static char *generate_asm64 (const Module *m, const char *src)
{
  Out o;
  char *helpline, *hb;
  int has_svc = m->service != NULL, has_swi, per_swi, i, k, ncmds = m->ncmds;
  unsigned *nums = NULL;
  int nn = 0, share = 0, notable = m->swi_notable != 0;

  per_swi = any_swi_function (m);
  has_swi = m->swi_handler != NULL || per_swi;
  helpline = help_line (m);
  if (m->nsvc)
    {
      nums = xmalloc (sizeof (unsigned) * (size_t) m->nsvc);
      memcpy (nums, m->svc, sizeof (unsigned) * (size_t) m->nsvc);
      qsort (nums, (size_t) m->nsvc, sizeof (unsigned), cmp_unsigned64);
      for (i = 0; i < m->nsvc; i++) if (i == 0 || nums[i] != nums[i - 1]) nums[nn++] = nums[i];
    }
  if (has_swi && m->swi_prefix && !notable && strlen (m->swi_prefix) == strlen (m->title))
    {
      share = 1;
      for (k = 0; m->swi_prefix[k]; k++) if (tolower ((unsigned char) m->swi_prefix[k]) != tolower ((unsigned char) m->title[k])) share = 0;
    }
  out_init (&o);
  A (&o, "/* Generated by cmunge64.py from %s.  The header of a 64-bit module for the kit.  DO NOT EDIT. */", base_name (src));
  A (&o, "\t.section .text.header, \"ax\"\n\t.global\t_mod_start\n\t.hidden\t_mod_start\n_mod_start:");
  A (&o, "\t.word\t0\t\t\t\t\t/* 00 start entry: none */");
  A (&o, "\t.word\t(__mod_init_v - _mod_start) | 0x40000000\t/* 04 initialisation (bit 30: not ARM) */");
  A (&o, "\t.word\t__mod_final_v - _mod_start\t\t/* 08 finalisation */");
  A (&o, "\t.word\t%s\t\t/* 0C service call handler */", has_svc ? "mod_service - _mod_start" : "0");
  A (&o, "\t.word\ttitle - _mod_start\n\t.word\thelp - _mod_start");
  A (&o, "\t.word\t%s\t\t\t/* 18 commands */", ncmds ? "cmdtab - _mod_start" : "0");
  A (&o, "\t.word\t%s\t\t\t\t/* 1C SWI chunk base */", hx (has_swi ? m->swi_chunk : 0));
  A (&o, "\t.word\t%s\t\t/* 20 SWI handler */", has_swi ? "__mod_swi_v - _mod_start" : "0");
  A (&o, "\t.word\t%s\t\t/* 24 SWI decoding table */", share ? "title - _mod_start" : (has_swi && m->swi_prefix && !notable) ? "swi_table - _mod_start" : "0");
  A (&o, "\t.word\t%s", m->swi_dec ? "__mod_decode_v - _mod_start\t\t/* 28 SWI decoding code */" : "0\t\t\t\t\t/* 28 SWI decoding code: none */");
  A (&o, "\t.word\t%s\t\t/* 2C messages file */", m->mfile ? "msgfile - _mod_start" : "0");
  A (&o, "\t.word\tmod_flags - _mod_start\t\t\t/* 30 offset to the module flags */");
  A (&o, "title:\n\t.asciz\t\"%s\"", m->title);
  if (share)
    {
      for (i = 0; i < m->nswi; i++) A (&o, "\t.asciz\t\"%s\"", m->swi_names[i]);
      A (&o, "\t.byte\t0\t\t\t\t\t/* end of the SWI table */");
    }
  hb = asm_bytes ((const unsigned char *) helpline, strlen (helpline));
  A (&o, "help:\n%s\n\t.byte\t0\n\t.balign\t4", hb);
  free (hb);
  if (m->mfile)
    {
      char *s = asm_bytes (m->mfile, m->mfilelen);
      A (&o, "msgfile:\n%s\n\t.byte\t0\n\t.balign\t4", s);
      free (s);
    }
  if (has_swi && m->swi_prefix && !share && !notable)
    {
      A (&o, "swi_table:\n\t.asciz\t\"%s\"", m->swi_prefix);
      for (i = 0; i < m->nswi; i++) A (&o, "\t.asciz\t\"%s\"", m->swi_names[i]);
      A (&o, "\t.byte\t0\n\t.balign\t4");
    }
  if (ncmds)
    {
      for (i = 0; i < ncmds; i++)
        {
          const Cmd *c = &m->cmds[i];
          if (c->has_help)
            {
              char *s = asm_bytes (c->help, c->helplen);
              if (c->add_syntax && c->has_syntax) A (&o, "ht%d:\n%s", i, s);
              else A (&o, "ht%d:\n%s\n\t.byte\t0", i, s);
              free (s);
            }
          if (c->has_syntax) { char *s = asm_bytes (c->syntax, c->synlen); A (&o, "is%d:\n%s\n\t.byte\t0", i, s); free (s); }
        }
      A (&o, "\t.balign\t4\ncmdtab:");
      for (i = 0; i < ncmds; i++)
        {
          const Cmd *c = &m->cmds[i];
          unsigned info = c->min | (c->gstrans << 8) | (c->max << 16) | ((unsigned) c->fs_command << 31) | ((unsigned) c->status_cmd << 30) | ((unsigned) c->international << 28);
          char buf[64];
          A (&o, "\t.asciz\t\"%s\"\n\t.balign\t4", c->name);
          if (c->nohandler) A (&o, "\t.word\t0\t\t\t\t\t/* no handler (no-handler:) */");
          else { sprintf (buf, "cmd%d - _mod_start", i); A (&o, "\t.word\t%s", buf); }
          A (&o, "\t.word\t%s\t\t\t\t/* min %u, max %u parameters */", hx (info), c->min, c->max);
          if (c->has_syntax) { sprintf (buf, "is%d - _mod_start", i); A (&o, "\t.word\t%s", buf); } else A (&o, "\t.word\t0");
          if (c->has_help) { sprintf (buf, "ht%d - _mod_start", i); A (&o, "\t.word\t%s", buf); } else A (&o, "\t.word\t0");
        }
      A (&o, "\t.word\t0\t\t\t\t\t/* end of the table */");
    }
  A (&o, "\t.balign\t4\nmod_flags:\n\t.word\t0x14\t\t\t\t\t/* bit 0 clear; bit 2: a zero-init size follows; bits 4-7 = 1: AArch64 */\n\t.word\t__ZISize");
  /* commands */
  {
    int nhandled = 0;
    for (i = 0; i < ncmds; i++) if (!m->cmds[i].nohandler) nhandled++;
    if (nhandled)
      {
        const char *dflt = m->cmd_handler;
        for (i = 0; i < ncmds; i++) if (!m->cmds[i].nohandler) A (&o, "cmd%d:\tmov\tx2, #%d\n\tb\t__mod_cmd_v", i, i);
        A (&o, "\t.global\tmodule_command\n\t.hidden\tmodule_command\n\t.type\tmodule_command, %%function\nmodule_command:\t\t\t\t\t/* x0 = command, x1 = args, x2 = argc, x3 = pw  ->  FN (args, argc, command, pw) */");
        A (&o, "\tmov\tx4, x0\n\tmov\tx0, x1\n\tmov\tx1, x2\n\tmov\tx2, x4");
        for (i = 0; i < ncmds; i++)
          {
            const Cmd *c = &m->cmds[i];
            if (c->nohandler) continue;
            if (c->handler && c->handler[0] && (!dflt || strcmp (c->handler, dflt) != 0)) A (&o, "\tcmp\tw4, #%d\n\tb.ne\t1f\n\tb\t%s\n1:", i, c->handler);
          }
        if (dflt) A (&o, "\tb\t%s", dflt); else A (&o, "\tmov\tx0, #0\n\tret");
      }
  }
  if (m->init) A (&o, "\t.global\tmodule_init\n\t.hidden\tmodule_init\n\t.type\tmodule_init, %%function\nmodule_init:\n\tb\t%s", m->init);
  if (m->final) A (&o, "\t.global\tmodule_final\n\t.hidden\tmodule_final\n\t.type\tmodule_final, %%function\nmodule_final:\n\tb\t%s", m->final);
  if (has_svc) A (&o, "\t.global\tmodule_service\n\t.hidden\tmodule_service\n\t.type\tmodule_service, %%function\nmodule_service:\n\tb\t%s", m->service);
  if (has_swi)
    {
      A (&o, "\t.global\tmodule_swi\n\t.hidden\tmodule_swi\n\t.type\tmodule_swi, %%function\nmodule_swi:\t\t\t\t\t/* x0 = SWI offset, x1 = registers, x2 = pw */");
      if (per_swi)
        for (i = 0; i < m->nswi; i++)
          {
            const char *h = m->swi_handlers[i];
            if (h && h[0] && (!m->swi_handler || strcmp (h, m->swi_handler) != 0)) A (&o, "\tcmp\tw0, #%d\n\tb.ne\t1f\n\tb\t%s\n1:", i, h);
          }
      if (m->swi_handler) A (&o, "\tb\t%s", m->swi_handler);
      else A (&o, "\tmov\tx0, #-1\t\t\t\t/* error_BAD_SWI: the kernel makes the error */\n\tret");
    }
  if (m->cxx)
    A (&o, "\t.global\t__mod_cxx_init\n\t.hidden\t__mod_cxx_init\n\t.type\t__mod_cxx_init, %%function\n__mod_cxx_init:\n\tb\t__modlib_cxx_init\n\t.global\t__mod_cxx_fini\n\t.hidden\t__mod_cxx_fini\n\t.type\t__mod_cxx_fini, %%function\n__mod_cxx_fini:\n\tb\t__modlib_cxx_fini");
  if (m->swi_dec)
    {
      if (!m->swi_dec2) A (&o, "\t.global\tmodule_swi_decode\n\t.hidden\tmodule_swi_decode\n\t.type\tmodule_swi_decode, %%function\nmodule_swi_decode:\n\tb\t%s", m->swi_dec);
      else A (&o, "\t.global\tmodule_swi_name\n\t.hidden\tmodule_swi_name\n\t.type\tmodule_swi_name, %%function\nmodule_swi_name:\n\tb\t%s\n\t.global\tmodule_swi_number\n\t.hidden\tmodule_swi_number\n\t.type\tmodule_swi_number, %%function\nmodule_swi_number:\n\tb\t%s", m->swi_dec, m->swi_dec2);
      A (&o, "\t.section .rodata\n\t.balign 4\n\t.global\t__mod_decode_form\n\t.hidden\t__mod_decode_form\n__mod_decode_form:\n\t.word\t%d\n\t.section .text.header, \"ax\"", m->swi_dec2 ? 2 : 1);
    }
  if (m->nerr)
    {
      unsigned n = m->error_base;
      A (&o, "\t.section .rodata\n\t.balign 4");
      for (i = 0; i < m->nerr; i++)
        {
          unsigned use = m->errs[i].num ? m->errs[i].num : n++;
          char *sb = asm_bytes (m->errs[i].msg, m->errs[i].len);
          A (&o, "\t.global\t__err_%s\n\t.hidden\t__err_%s\n__err_%s:\n\t.word\t%s\n%s\n\t.byte\t0\n\t.balign\t4", m->errs[i].name, m->errs[i].name, m->errs[i].name, hx (use), sb);
          free (sb);
        }
      A (&o, "\t.section .text.header, \"ax\"");
    }
  A (&o, "#include \"modcore64.S\"");
  A (&o, "\t.section .text.header, \"ax\"");
  if (has_svc)
    {
      if (nn)
        {
          Buf b; buf_init (&b);
          for (i = 0; i < nn; i++) { if (i) buf_adds (&b, ", "); buf_adds (&b, hx (nums[i])); }
          A (&o, "\tMOD_SERVICE_PLAIN %s", b.s);
          free (b.s);
        }
      else A (&o, "\tMOD_SERVICE_PLAIN");
    }
  else A (&o, "\t.global\t__mod_svc_numbers\n\t.hidden\t__mod_svc_numbers\n__mod_svc_numbers:\n\t.word\t0\t\t\t\t\t/* no services: the kit's runtime refers to the list */");
  buf_addc (&o.b, '\n');
  free (helpline); free (nums);
  return o.b.s;
}

static char *generate_h (const Module *m, const char *src)
{
  Out o;
  Help h;
  int i;
  Buf guard;
  const char *p;

  split_help (m->help, &h);
  buf_init (&guard);
  buf_adds (&guard, "_MODKIT_");
  for (p = m->title; *p; p++) buf_addc (&guard, is_word ((unsigned char) *p) ? *p : '_');
  buf_adds (&guard, "_H_");
  out_init (&o);
  A (&o, "/* Generated by mkmodhdr.py from %s.  DO NOT EDIT. */", base_name (src));
  A (&o, "#ifndef %s\n#define %s\n\n#include \"kernel.h\"\n", guard.s, guard.s);
  A (&o, "#define Module_Title\t\t\"%s\"\n#define Module_Help\t\t\"%s\"\n#define Module_VersionString\t\"%u.%02u\"\n#define Module_VersionNumber\t%u\n#ifndef Module_Date\n#define Module_Date\t\t\"%s\"\n#endif\n", m->title, h.name, h.vnum / 100, h.vnum % 100, h.vnum, m->date ? m->date : "");
  if (m->mfile)
    {
      /* the name as a C string (the quotes and backslashes of the CMHG string are escaped again) */
      Buf q;
      size_t k;
      buf_init (&q);
      for (k = 0; k < m->mfilelen; k++) { unsigned char c = m->mfile[k]; if (c == '"' || c == '\\') buf_addc (&q, '\\'); buf_addc (&q, (char) c); }
      A (&o, "#define Module_MessagesFile\t\"%s\"\n", q.s);
      free (q.s);
    }
  A (&o, "#ifdef __cplusplus\nextern \"C\" {\n#endif\n");
  if (m->base) A (&o, "extern const int Image__RO_Base;\n");
  if (m->init) A (&o, "_kernel_oserror *%s (const char *tail, int podule_base, void *pw);", m->init);
  if (m->final) A (&o, "_kernel_oserror *%s (int fatal, int podule_base, void *pw);", m->final);
  if (m->ncmds)
    {
      int j, k;
      for (i = -1; i < m->ncmds; i++)                       /* the table's handler, then the commands' own functions, each once */
        {
          const char *fn = i < 0 ? m->cmd_handler : m->cmds[i].handler;
          if (!fn) continue;
          for (j = -1; j < i; j++)
            {
              const char *g = j < 0 ? m->cmd_handler : m->cmds[j].handler;
              if (g && strcmp (g, fn) == 0) break;
            }
          k = j < i;
          if (!k) A (&o, "_kernel_oserror *%s (const char *arg_string, int argc, int number, void *pw);", fn);
        }
      A (&o, "#define help_PRINT_BUFFER\t\t((_kernel_oserror *) arg_string)\n#define arg_CONFIGURE_SYNTAX\t\t((char *) 0)\n#define arg_STATUS\t\t\t((char *) 1)\n#define configure_BAD_OPTION\t\t((_kernel_oserror *) -1)\n#define configure_NUMBER_NEEDED\t\t((_kernel_oserror *) 1)\n#define configure_TOO_LARGE\t\t((_kernel_oserror *) 2)\n#define configure_TOO_MANY_PARAMS\t((_kernel_oserror *) 3)\n");
      A (&o, "/* Command numbers, as passed to the command handler function */");
      for (i = 0; i < m->ncmds; i++) A (&o, "#undef CMD_%s\n#define CMD_%s (%d)", m->cmds[i].name, m->cmds[i].name, i);
      A (&o, "%s", "");
    }
  if (m->service) A (&o, "void %s (int service_number, _kernel_swi_regs *r, void *pw);", m->service);
  if (m->swi_dec)
    {
      if (!m->swi_dec2) A (&o, "int %s (_kernel_swi_regs *r, void *pw);", m->swi_dec);
      else A (&o, "int %s (const char *name, void *pw);\nint %s (int number, char *buffer, int offset, int limit, void *pw);", m->swi_dec, m->swi_dec2);
    }
  if (m->swi_handler || any_swi_function (m))
    {
      if (m->swi_handler) A (&o, "_kernel_oserror *%s (int swi_offset, _kernel_swi_regs *r, void *pw);", m->swi_handler);
      {
        const char **fns = xmalloc (sizeof (char *) * (size_t) (m->nswi + 1));
        int nf = 0, j;
        for (i = 0; i < m->nswi; i++) if (m->swi_handlers[i]) fns[nf++] = m->swi_handlers[i];
        for (i = 0; i < nf; i++) for (j = i + 1; j < nf; j++) if (strcmp (fns[j], fns[i]) < 0) { const char *t = fns[i]; fns[i] = fns[j]; fns[j] = t; }
        for (i = 0; i < nf; i++) if (i == 0 || strcmp (fns[i], fns[i - 1]) != 0) A (&o, "_kernel_oserror *%s (int swi_offset, _kernel_swi_regs *r, void *pw);", fns[i]);
        free (fns);
      }
      A (&o, "#define Module_SWIChunk\t\t%s", hx (m->swi_chunk));
      A (&o, "\n/* SWI number definitions (as CMunge writes them) */\n#define %s_00 (%s)", m->swi_prefix, hx (m->swi_chunk));
      for (i = 0; i < m->nswi; i++)
        {
          char x[16];
          snprintf (x, sizeof x, "%s", hx (m->swi_chunk + (unsigned) i));
          A (&o, "#undef %s_%s\n#undef X%s_%s\n#define %s_%s\t\t(%s)\n#define X%s_%s\t\t(%s)", m->swi_prefix, m->swi_names[i], m->swi_prefix, m->swi_names[i], m->swi_prefix, m->swi_names[i], x,
             m->swi_prefix, m->swi_names[i], hx (m->swi_chunk + (unsigned) i + 0x20000u));
        }
      A (&o, "\n/* Special error for 'SWI values out of range for this module' */\n#define error_BAD_SWI ((_kernel_oserror *) -1)");
    }
    {
    int any_trap = 0, any_other = 0, k;
    for (k = 0; k < m->nven; k++) { if (strcmp (m->ven[k].kind, "vector-traps") == 0) any_trap = 1; else any_other = 1; }
    if (any_trap)
      A (&o, "\n/* The function that a vector trap's handler calls to run the rest of the vector chain with the registers of REGS: the result is NULL, or an error block (the chain claimed the vector with V set) */\ntypedef _kernel_oserror *(*vectortrap_f) (_kernel_swi_regs *regs, void *trappw);");
    for (i = 0; i < m->nven; i++)
      {
        if (strcmp (m->ven[i].kind, "vector-traps") == 0)
          A (&o, "extern void %s (void);\t\t/* the entry point of the vector: not a C function */\n_kernel_oserror *%s (_kernel_swi_regs *r, void *pw, vectortrap_f trap, void *trappw);", m->ven[i].entry, m->ven[i].handler);
        else
          A (&o, "extern void %s (void);\n%s %s (_kernel_swi_regs *r, void *pw);", m->ven[i].entry, strcmp (m->ven[i].kind, "generic-veneers") == 0 ? "_kernel_oserror *" : "int", m->ven[i].handler);        /* (CMunge: a generic handler returns an error) */
      }
    if (any_trap)
      A (&o, "\n/* VECTORTRAP_PASSON should only be returned to pass the call on when you have no interest in it: it is quicker than calling TRAP and returning its result; NULL claims the vector, an error block claims it with V set */\n#define VECTORTRAP_PASSON ((_kernel_oserror *) 1)");
    if (m->pd_entry)
      {
        A (&o, "\nextern void %s (void);\t\t/* the value to give as r0 to PDriver_DeclareDriver: not a C function */", m->pd_entry);
        A (&o, "/* Functions called to handle the PDriver SWI calls, one for each reason code from 0 */");
        for (k = 0; k < m->npd; k++)
          {
            int j, seen = 0;
            for (j = 0; j < k; j++) if (strcmp (m->pd_names[j], m->pd_names[k]) == 0) seen = 1;
            if (!seen) A (&o, "_kernel_oserror *%s (int number, _kernel_swi_regs *r, void *pw);", m->pd_names[k]);
          }
        A (&o, "\n/* Return NULL if the call was handled (r0 - r9 as returned in R), an error block, or error_BAD_PDRIVER_SWI for a reason code that this module does not handle */\n#define error_BAD_PDRIVER_SWI ((_kernel_oserror *) -1)");
      }
    (void) any_other;
  }
  {
    int any_carry = 0, any_err = 0, nonvt = 0;
    for (i = 0; i < m->nven; i++)
      {
        if (strcmp (m->ven[i].kind, "vector-traps") != 0) nonvt = 1;
        if (strcmp (m->ven[i].kind, "generic-veneers") == 0 && m->ven[i].carry) any_carry = 1;
        if (m->ven[i].errc) any_err = 1;
      }
    if (any_carry)
      A (&o, "\n/* VENEER_SETCARRY can be returned from a generic veneer's function that is carry-capable: return with C set, all other registers as the function left them */\n#define VENEER_SETCARRY ((_kernel_oserror *) 2)");
    if (nonvt) A (&o, "\n/* VECTOR_PASSON can be returned from vectors to pass the call on to other claimants; VECTOR_CLAIM to claim it. */\n#define VECTOR_PASSON (1)\n#define VECTOR_CLAIM (0)");
    if (m->nven && any_err)
      A (&o, "\n/* VECTOR_ERROR (err) can be returned from a vector handler that is error-capable: claim the vector and return with V set and r0 = the error block */\n#define VECTOR_ERROR(err) ((int) (err))");
  }
  if (m->error_base) A (&o, "\n/* Error base for this module */\n#define ERROR_BASE (0x%08x)", m->error_base);
  if (m->nerr)
    {
      unsigned n = m->error_base;
      int i;
      A (&o, "/* Error messages embedded in header, based at &%x */", m->error_base);
      for (i = 0; i < m->nerr; i++)
        {
          unsigned use = m->errs[i].num ? m->errs[i].num : n++;
          A (&o, "extern void __err_%s(void); /* &%x */", m->errs[i].name, use);
        }
      for (i = 0; i < m->nerr; i++) A (&o, "#define %s ((_kernel_oserror*)(int)__err_%s)", m->errs[i].name, m->errs[i].name);
    }
  A (&o, "\n#ifdef __cplusplus\n}\n#endif\n#endif");
  buf_addc (&o.b, '\n');
  free (h.name); free (h.version); free (h.rest); free (guard.s);
  return o.b.s;
}


/* ---------------------------------------------------------------- the command line */
static const char *argv0dir;

static int file_exists (const char *p)
{
  FILE *f = fopen (p, "rb");
  if (f) fclose (f);
  return f != NULL;
}

/* the compiler that preprocesses and assembles: CMUNGE64_CC, else aarch64-riscos-gcc next to this program (when it is found there), else the one on the search path */
static char *find_cc (void)
{
  const char *cc = getenv ("CMUNGE64_CC");
  if (cc && *cc) return xstrdup (cc);
#if !defined (__riscos__)
  if (argv0dir && *argv0dir)
    {
      Buf b;
      buf_init (&b);
      buf_printf (&b, "%s/aarch64-riscos-gcc", argv0dir);
      if (file_exists (b.s)) return b.s;
      free (b.s);
    }
#endif
  return xstrdup ("aarch64-riscos-gcc");
}

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

static char *temps[2];
static int ntemps;
static void remove_temps (void) { while (ntemps > 0) remove (temps[--ntemps]); }

static char *temp_name (const char *tag, const char *ext)
{
  const char *dir = getenv ("TMPDIR");
  Buf b;
  static unsigned counter;
  unsigned long pid = (unsigned long) getpid ();
  buf_init (&b);
#if defined (__linux__)
  buf_printf (&b, "%s/cmunge64-%lu%s%u%s", dir && *dir ? dir : "/tmp", pid, tag, counter++, ext);
#else
  buf_printf (&b, "%s/cmunge64-%lu%s%u%s", dir && *dir ? dir : ".", pid, tag, counter++, ext);
#endif
  if (ntemps == 0) atexit (remove_temps);
  if (ntemps < 2) temps[ntemps++] = b.s;
  return b.s;
}

static char *str_replace (const char *s, const char *a, const char *b)
{
  Buf o;
  size_t al = strlen (a);
  buf_init (&o);
  for (;;)
    {
      const char *p = strstr (s, a);
      if (!p) { buf_adds (&o, s); break; }
      buf_addn (&o, s, (size_t) (p - s));
      buf_adds (&o, b);
      s = p + al;
    }
  return o.s;
}

/* the name without its last extension (Python's os.path.splitext, for a name that may have directories) */
static char *without_ext (const char *f)
{
  const char *slash = strrchr (f, '/'), *dot = strrchr (f, '.');
  if (!dot || (slash && dot < slash) || dot == f || (slash && dot == slash + 1)) return xstrdup (f);
  return xstrndup (f, (size_t) (dot - f));
}

int main (int argc, char **argv)
{
  const char *out_o = NULL, *out_s = NULL, *out_h = NULL, *infile = NULL;
  int pre = 0, i, ncpp = 0;
  char **cpp = xmalloc (sizeof (char *) * (size_t) (argc + 1));
  char *srctext, *src, *asm_text, *h_text, *h2, *h3, *base, *cc, *ppfile = NULL, *asmfile;
  Module m;
  const char *slash;

  progname = "cmunge64";
  slash = strrchr (argv[0], '/');
  argv0dir = slash ? xstrndup (argv[0], (size_t) (slash - argv[0])) : "";
#if defined (__linux__)
  {
    char exe[4097];
    ssize_t n = readlink ("/proc/self/exe", exe, 4096);
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
      if (!strcmp (a, "-o") || !strcmp (a, "-s") || !strcmp (a, "-d"))
        {
          if (++i >= argc) die ("%s needs a file name", a);
          if (a[1] == 'o') out_o = argv[i]; else if (a[1] == 's') out_s = argv[i]; else out_h = argv[i];
        }
      else if (!strcmp (a, "-p")) pre = 1;
      else if (a[0] == '-' && (a[1] == 'D' || a[1] == 'U' || a[1] == 'I') && a[2]) cpp[ncpp++] = (char *) a;
      else if (!strcmp (a, "-h") || !strcmp (a, "--help")) { puts ("cmunge64 [-p] [-D<sym>[=<val>]] [-U<sym>] [-I<dir>] [-s FILE.S] [-d FILE.h] [-o FILE.o] FILE.cmhg"); return 0; }
      else if (a[0] == '-') die ("unknown option %s", a);
      else if (!infile) infile = a;
      else die ("more than one input file");
    }
  if (!infile) die ("no input file (cmunge64 -h)");

  src = (char *) infile;
  if (pre)
    {
      Buf cmd;
      cc = find_cc ();
      ppfile = temp_name ("p", "");
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
  {
    Buf bad; int nbad = 0;
    buf_init (&bad);
    if (m.nven) { buf_adds (&bad, "irq-handlers/vector-handlers/generic-veneers/vector-traps/event-handler"); nbad++; }
    if (m.pd_entry) { if (nbad++) buf_adds (&bad, ", "); buf_adds (&bad, "pdriver-handler"); }
    if (m.runnable) { if (nbad++) buf_adds (&bad, ", "); buf_adds (&bad, "module-is-runnable"); }
    if (nbad) die ("%s: %s: not supported for AArch64 yet", infile, bad.s);
  }
  asm_text = generate_asm64 (&m, infile);
  h_text = generate_h (&m, infile);
  h2 = str_replace (h_text, "extern void __err_", "extern void __attribute__ ((visibility (\"hidden\"))) __err_");
  h3 = str_replace (h2, "(_kernel_oserror*)(int)__err_", "(_kernel_oserror*)(long)__err_");
  base = without_ext (infile);
  if (!out_s && !out_o) { Buf b; buf_init (&b); buf_printf (&b, "%s_hdr.S", base); out_s = b.s; }
  asmfile = (char *) out_s;
  if (!asmfile) asmfile = temp_name ("s", ".S");
  if (asmfile) write_file (asmfile, asm_text, strlen (asm_text));
  if (out_o)
    {
      Buf cmd;
      cc = find_cc ();
      buf_init (&cmd);
      buf_adds (&cmd, cc);
      add_arg (&cmd, "-mmodule"); add_arg (&cmd, "-c"); add_arg (&cmd, asmfile); add_arg (&cmd, "-o"); add_arg (&cmd, out_o);
      run (&cmd, "the assembler");
    }
  if (!out_h) { Buf b; buf_init (&b); buf_printf (&b, "%s_hdr.h", base); out_h = b.s; }
  write_file (out_h, h3, strlen (h3));
  return 0;
}
