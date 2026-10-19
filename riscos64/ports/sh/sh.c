/* sh.c - a POSIX shell for RISC OS 64 (UnixLib 64), written for a system that has no fork: every command is run to its end before the next one starts.
   What it does: the shell language (lists, && ||, pipelines, if/while/until/for/case, functions, { } and ( ), here documents, quoting, $var ${var..} $(...) `...` $((...)), pathname expansion, tilde), the redirections
   < > >> << <& >&, and builtins that the programs of a build need (cd echo printf test [ read export set shift eval . exit return break continue unset local trap wait type command exec umask ulimit
   expr, and rm cp mv mkdir rmdir cat touch basename dirname sleep chmod, because RISC OS has no such programs).
   How: a program is started with OS_CLI (system ()); its screen output goes to a file with  { > file }  if the operating system takes that (RISC OS 5), else with *Spool, its keyboard input comes from a file with
   { < file } or *Exec.  A pipeline is run stage after stage through scrap files.  ( ... ) and the & of a command run the commands in this shell and at once (a ( ) group keeps its variables and folder to itself).
   Standard error of a program that is linked with the SDK goes where the system variable Sh$Stderr says (this shell sets it for  2>file  and  2>/dev/null);  2>&1 puts it with the output.  Programs
   that are not linked with the SDK (the commands of the operating system) write their errors to the screen with their output.
   Usage: sh [-ex] [-c command [name arg...]] [script arg...]    The SDK of RISC OS 64 (BSD licence like the rest of riscos64). */
#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <glob.h>
#include <limits.h>
#include <regex.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>

extern char **environ;
extern const char *__riscos_path (const char *name, char *buf, int create_dir);
extern int __ul64_system_raw (const char *cmd);	/* the command line to the operating system, as it is (system () would use this shell again) */

/* ------------------------------------------------------------------------------------------------------ strings */
typedef struct { char *s; size_t n, cap; } str;
static void sb_init (str *b) { b->s = 0; b->n = b->cap = 0; }
static void sb_reserve (str *b, size_t more)
{
  if (b->n + more + 1 > b->cap)
    {
      size_t c = b->cap ? b->cap * 2 : 64;
      while (c < b->n + more + 1) c *= 2;
      b->s = realloc (b->s, c);
      if (!b->s) { fputs ("sh: out of memory\n", stderr); exit (2); }
      b->cap = c;
    }
}
static void sb_add (str *b, const char *p, size_t n) { sb_reserve (b, n); memcpy (b->s + b->n, p, n); b->n += n; b->s[b->n] = 0; }
static void sb_addc (str *b, char c) { sb_add (b, &c, 1); }
static void sb_adds (str *b, const char *s) { sb_add (b, s, strlen (s)); }
static char *sb_take (str *b) { char *s = b->s ? b->s : strdup (""); sb_init (b); return s; }
static void sb_free (str *b) { free (b->s); sb_init (b); }

static void die (const char *fmt, ...)
{
  va_list ap;
  fflush (stdout);
  fputs ("sh: ", stderr);
  va_start (ap, fmt); vfprintf (stderr, fmt, ap); va_end (ap);
  fputc ('\n', stderr);
  exit (2);
}

typedef struct { char **v; int n, cap; } list;
static void l_add (list *l, char *s)
{
  if (l->n + 2 > l->cap) { l->cap = l->cap ? l->cap * 2 : 8; l->v = realloc (l->v, (size_t) l->cap * sizeof *l->v); }
  l->v[l->n++] = s; l->v[l->n] = 0;
}

/* ------------------------------------------------------------------------------------------------------ variables */
struct var { char *name, *value; int exported, readonly, local; struct var *next; };
static struct var *vars;
static char **pos_params; static int npos;		/* $1 ... */
static char *arg0;
static int last_status, bg_pid;
/* background jobs run to their end at once: their status is kept for  wait PID  */
static struct { int pid, status; } jobs_[64];
static void job_add (int pid, int status) { jobs_[pid % 64].pid = pid; jobs_[pid % 64].status = status; }
static int job_known (int pid) { return pid > 0 && jobs_[pid % 64].pid == pid; }
static int job_status (int pid) { return job_known (pid) ? jobs_[pid % 64].status : 127; }
static int opt_e, opt_x, opt_u, opt_f, opt_n, opt_pipefail;

static struct var *find_var (const char *name)
{
  struct var *v;
  for (v = vars; v; v = v->next) if (!strcmp (v->name, name)) return v;
  return 0;
}
static const char *get_var (const char *name) { struct var *v = find_var (name); return v ? v->value : 0; }
static void sync_env (struct var *v)
{
  if (v->exported && v->value) setenv (v->name, v->value, 1);
  else if (!v->value || !v->exported) unsetenv (v->name);
}
struct arr;
static struct arr *find_arr (const char *name);
static void arr_set (struct arr *a, int idx, const char *val);
static int arr_syncing;
static int set_var (const char *name, const char *value)
{
  struct var *v = find_var (name);
  if (v && v->readonly) { fprintf (stderr, "sh: %s: is read only\n", name); return 1; }
  if (!v) { v = calloc (1, sizeof *v); v->name = strdup (name); v->next = vars; vars = v; }
  free (v->value);
  v->value = value ? strdup (value) : 0;
  if (v->exported) sync_env (v);
  if (!arr_syncing && value) { struct arr *a = find_arr (name); if (a) { arr_syncing++; arr_set (a, 0, value); arr_syncing--; } }
  return 0;
}
static void export_var (const char *name, const char *value)
{
  struct var *v = find_var (name);
  if (value) set_var (name, value);
  v = find_var (name);
  if (!v) { v = calloc (1, sizeof *v); v->name = strdup (name); v->value = 0; v->next = vars; vars = v; }
  v->exported = 1;
  if (v->value) sync_env (v);
}
static void arr_remove (const char *name);
static void unset_var (const char *name)
{
  struct var *v = find_var (name);
  if (!v || v->readonly) return;
  free (v->value); v->value = 0;
  unsetenv (name);
  arr_remove (name);
}
/* ---- indexed arrays (bash style): a[i]=v  a=(x y z)  a+=(w)  ${a[i]} ${a[@]} ${#a[@]} ${!a[@]}  unset a[i].  The scalar variable of the same name always holds element 0, so $a works. ---- */
struct arr { char *name; char **v; int n; struct arr *next; };	/* v[i] is NULL for an index that is not set */
static struct arr *arrays;
static struct arr *find_arr (const char *name)
{
  struct arr *a;
  for (a = arrays; a; a = a->next) if (!strcmp (a->name, name)) return a;
  return 0;
}
static void arr_clear (struct arr *a) { int i; for (i = 0; i < a->n; i++) free (a->v[i]); free (a->v); a->v = 0; a->n = 0; }
static int set_var (const char *name, const char *value);
static void arr_sync (struct arr *a)
{
  int i;
  arr_syncing++;
  for (i = 0; i < a->n && !a->v[i]; i++) ;
  if (i < a->n) set_var (a->name, a->v[i]); else set_var (a->name, "");
  arr_syncing--;
}
static struct arr *make_arr (const char *name)
{
  struct arr *a = find_arr (name);
  const char *old;
  if (a) return a;
  a = calloc (1, sizeof *a); a->name = strdup (name); a->next = arrays; arrays = a;
  old = get_var (name);
  if (old) { a->v = calloc (1, sizeof *a->v); a->v[0] = strdup (old); a->n = 1; }
  return a;
}
static void arr_set (struct arr *a, int idx, const char *val)
{
  if (idx < 0) idx = 0;
  if (idx >= a->n) { a->v = realloc (a->v, (size_t) (idx + 1) * sizeof *a->v); memset (a->v + a->n, 0, (size_t) (idx + 1 - a->n) * sizeof *a->v); a->n = idx + 1; }
  free (a->v[idx]); a->v[idx] = strdup (val);
  arr_sync (a);
}
static int arr_count (struct arr *a) { int i, c = 0; for (i = 0; i < a->n; i++) if (a->v[i]) c++; return c; }
static void arr_remove (const char *name)
{
  struct arr **pp;
  for (pp = &arrays; *pp; pp = &(*pp)->next)
    if (!strcmp ((*pp)->name, name)) { struct arr *a = *pp; *pp = a->next; arr_clear (a); free (a->name); free (a); return; }
}

static int valid_name (const char *s)
{
  if (!(isalpha ((unsigned char) *s) || *s == '_')) return 0;
  while (*++s) if (!(isalnum ((unsigned char) *s) || *s == '_')) return 0;
  return 1;
}

/* ------------------------------------------------------------------------------------------------------ the syntax tree */
enum { N_SIMPLE, N_PIPE, N_AND, N_OR, N_LIST, N_BG, N_IF, N_WHILE, N_UNTIL, N_FOR, N_CASE, N_FUNC, N_GROUP, N_SUBSHELL, N_NOT, N_DBRACKET };
enum { R_IN, R_OUT, R_APPEND, R_DUPIN, R_DUPOUT, R_HERE, R_RW };
struct redir { int fd, type; char *target; char *body; int quoted; struct redir *next; };
struct caseitem { list pats; struct node *body; struct caseitem *next; };
struct node
{
  int type;
  struct node *a, *b, *c;
  list words, assigns;
  int line;
  struct redir *redirs;
  char *name;
  struct caseitem *items;
  int has_in;				/* for: "in" was given */
};
static struct node *new_node (int t) { struct node *n = calloc (1, sizeof *n); n->type = t; return n; }

struct func { char *name; struct node *body; struct func *next; };
static struct func *funcs;

/* ------------------------------------------------------------------------------------------------------ the lexer */
enum { T_EOF, T_NL, T_WORD, T_IONUM, T_SEMI, T_DSEMI, T_AMP, T_AND, T_OR, T_PIPE, T_LP, T_RP, T_LESS, T_GREAT, T_DGREAT, T_DLESS, T_DLESSDASH, T_LESSAND, T_GREATAND, T_LESSGREAT };
struct heredoc_wait { struct redir *r; char *delim; int strip; struct heredoc_wait *next; };
struct parser { size_t line, tok_line; const char *s; size_t pos, len; int tok; char *word; int peeked; struct heredoc_wait *pending, **pend_tail; const char *name; char *no_alias; };

static int p_getc (struct parser *p)
{
  int c = p->pos < p->len ? (unsigned char) p->s[p->pos++] : -1;
  if (c == '\n') p->line++;
  return c;
}
static int p_peekc (struct parser *p) { return p->pos < p->len ? (unsigned char) p->s[p->pos] : -1; }

static void syntax (struct parser *p, const char *msg)
{
  size_t i, line = 1;
  for (i = 0; i < p->pos && i < p->len; i++) if (p->s[i] == '\n') line++;
  die ("%s: line %zu: syntax error: %s", p->name ? p->name : "?", line, msg);
}

/* read one line (for here documents) */
static char *p_line (struct parser *p)
{
  str b; int c;
  sb_init (&b);
  if (p->pos >= p->len) return 0;
  while ((c = p_getc (p)) >= 0 && c != '\n') sb_addc (&b, (char) c);
  if (!b.s) sb_adds (&b, "");
  return sb_take (&b);
}
static void read_heredocs (struct parser *p)
{
  struct heredoc_wait *h;
  for (h = p->pending; h; h = h->next)
    {
      str body; char *line;
      sb_init (&body);
      sb_adds (&body, "");
      while ((line = p_line (p)) != 0)
	{
	  const char *l = line;
	  if (h->strip) while (*l == '\t') l++;
	  if (!strcmp (l, h->delim)) { free (line); break; }
	  sb_adds (&body, l); sb_addc (&body, '\n');
	  free (line);
	}
      h->r->body = sb_take (&body);
    }
  p->pending = 0; p->pend_tail = &p->pending;
}

/* skip over a ${...}, $(...), `...` or $((...)) that starts at s[*i] (the character after the $ or the backquote), appending it to b */
static void lex_dollar (struct parser *p, str *b);
static void lex_quoted_until (struct parser *p, str *b, int close, int dq)
{
  int c;
  if (close == '\'')					/* inside single quotes nothing is special */
    {
      while ((c = p_getc (p)) >= 0) { sb_addc (b, (char) c); if (c == '\'') return; }
      syntax (p, "unterminated quote");
    }
  while ((c = p_getc (p)) >= 0)
    {
      sb_addc (b, (char) c);
      if (c == close) return;
      if (c == '\\') { int d = p_getc (p); if (d >= 0) sb_addc (b, (char) d); }
      else if (c == '\'' && !dq && close != '\'') { lex_quoted_until (p, b, '\'', 0); }
      else if (c == '"' && close != '"') { lex_quoted_until (p, b, '"', 1); }
      else if (c == '$') lex_dollar (p, b);
      else if (c == '`' && close != '`') lex_quoted_until (p, b, '`', dq);
    }
  syntax (p, "unterminated quote");
}
static void lex_dollar (struct parser *p, str *b)
{
  int c = p_peekc (p);
  if (c == '\'')					/* $'...': C escapes; \' does not end it */
    {
      sb_addc (b, (char) p_getc (p));
      while ((c = p_getc (p)) >= 0)
	{
	  sb_addc (b, (char) c);
	  if (c == '\\') { int d = p_getc (p); if (d >= 0) sb_addc (b, (char) d); }
	  else if (c == '\'') return;
	}
      syntax (p, "unterminated $'");
    }
  if (c == '(')
    {
      int depth = 0;
      while ((c = p_getc (p)) >= 0)
	{
	  sb_addc (b, (char) c);
	  if (c == '(') depth++;
	  else if (c == ')') { if (--depth == 0) return; }
	  else if (c == '\\') { int d = p_getc (p); if (d >= 0) sb_addc (b, (char) d); }
	  else if (c == '\'') lex_quoted_until (p, b, '\'', 0);
	  else if (c == '"') lex_quoted_until (p, b, '"', 1);
	  else if (c == '`') lex_quoted_until (p, b, '`', 0);
	}
      syntax (p, "unterminated $(");
    }
  else if (c == '{')
    {
      int depth = 0;
      while ((c = p_getc (p)) >= 0)
	{
	  sb_addc (b, (char) c);
	  if (c == '{') depth++;
	  else if (c == '}') { if (--depth == 0) return; }
	  else if (c == '\\') { int d = p_getc (p); if (d >= 0) sb_addc (b, (char) d); }
	  else if (c == '\'') lex_quoted_until (p, b, '\'', 0);
	  else if (c == '"') lex_quoted_until (p, b, '"', 1);
	  else if (c == '$') lex_dollar (p, b);
	}
      syntax (p, "unterminated ${");
    }
}

static int assignment_word (const char *w);
/* inside  name=( ... )  : read up to the matching ) */
static void lex_arraylit (struct parser *p, str *b)
{
  int depth = 1, c;
  while ((c = p_getc (p)) >= 0)
    {
      sb_addc (b, (char) c);
      if (c == '(') depth++;
      else if (c == ')') { if (--depth == 0) return; }
      else if (c == '\\') { int d = p_getc (p); if (d >= 0) sb_addc (b, (char) d); }
      else if (c == '\'') lex_quoted_until (p, b, '\'', 0);
      else if (c == '"') lex_quoted_until (p, b, '"', 1);
      else if (c == '$') lex_dollar (p, b);
    }
  syntax (p, "unterminated array assignment");
}
static int is_meta (int c) { return c == ' ' || c == '\t' || c == '\n' || c == ';' || c == '&' || c == '|' || c == '(' || c == ')' || c == '<' || c == '>'; }

static int lex (struct parser *p)
{
  int c;
  free (p->word); p->word = 0;
  for (;;)
    {
      c = p_peekc (p);
      if (c == ' ' || c == '\t') { p->pos++; continue; }
      if (c == '\\' && p->pos + 1 < p->len && p->s[p->pos + 1] == '\n') { p->pos += 2; p->line++; continue; }
      if (c == '#') { while ((c = p_peekc (p)) >= 0 && c != '\n') p->pos++; continue; }
      break;
    }
  c = p_peekc (p);
  p->tok_line = p->line;
  if (c < 0) return T_EOF;
  if (c == '\n') { p->pos++; p->line++; if (p->pending) read_heredocs (p); return T_NL; }
  p->pos++;
  switch (c)
    {
    case ';': if (p_peekc (p) == ';') { p->pos++; return T_DSEMI; } return T_SEMI;
    case '&': if (p_peekc (p) == '&') { p->pos++; return T_AND; } return T_AMP;
    case '|': if (p_peekc (p) == '|') { p->pos++; return T_OR; } return T_PIPE;
    case '(': return T_LP;
    case ')': return T_RP;
    case '<':
      if (p_peekc (p) == '<') { p->pos++; if (p_peekc (p) == '-') { p->pos++; return T_DLESSDASH; } return T_DLESS; }
      if (p_peekc (p) == '&') { p->pos++; return T_LESSAND; }
      if (p_peekc (p) == '>') { p->pos++; return T_LESSGREAT; }
      return T_LESS;
    case '>':
      if (p_peekc (p) == '>') { p->pos++; return T_DGREAT; }
      if (p_peekc (p) == '&') { p->pos++; return T_GREATAND; }
      if (p_peekc (p) == '|') { p->pos++; return T_GREAT; }
      return T_GREAT;
    }
  p->pos--;
  {
    str b; int digits = 1;
    sb_init (&b);
    while ((c = p_peekc (p)) >= 0 && (!is_meta (c) || (c == '(' && b.n > 0 && b.s[b.n - 1] == '=' && assignment_word (b.s))))
      {
	p->pos++;
	if (c == '(') { sb_addc (&b, '('); lex_arraylit (p, &b); digits = 0; continue; }
	if (!isdigit (c)) digits = 0;
	sb_addc (&b, (char) c);
	if (c == '\\') { int d = p_getc (p); if (d == '\n') b.n--; else if (d >= 0) sb_addc (&b, (char) d); b.s[b.n] = 0; }
	else if (c == '\'') lex_quoted_until (p, &b, '\'', 0);
	else if (c == '"') lex_quoted_until (p, &b, '"', 1);
	else if (c == '`') lex_quoted_until (p, &b, '`', 0);
	else if (c == '$') lex_dollar (p, &b);
      }
    p->word = sb_take (&b);
    if (digits && ((c = p_peekc (p)) == '<' || c == '>')) return T_IONUM;
    return T_WORD;
  }
}

static int peek (struct parser *p) { if (!p->peeked) { p->tok = lex (p); p->peeked = 1; } return p->tok; }
static void next (struct parser *p) { peek (p); p->peeked = 0; }
static int is_kw (struct parser *p, const char *kw) { return peek (p) == T_WORD && !strcmp (p->word, kw); }
static void skip_nl (struct parser *p) { while (peek (p) == T_NL) next (p); }

/* ------------------------------------------------------------------------------------------------------ the parser */
static struct node *parse_list (struct parser *p, int toplevel);
static struct node *parse_compound_list (struct parser *p);
static struct node *parse_command (struct parser *p);

static int is_redir_tok (int t) { return t == T_LESS || t == T_GREAT || t == T_DGREAT || t == T_DLESS || t == T_DLESSDASH || t == T_LESSAND || t == T_GREATAND || t == T_LESSGREAT; }

static struct redir *parse_redir (struct parser *p)
{
  struct redir *r = calloc (1, sizeof *r);
  int t;
  r->fd = -1;
  if (peek (p) == T_IONUM) { r->fd = atoi (p->word); next (p); }
  t = peek (p);
  next (p);
  switch (t)
    {
    case T_LESS: r->type = R_IN; if (r->fd < 0) r->fd = 0; break;
    case T_GREAT: r->type = R_OUT; if (r->fd < 0) r->fd = 1; break;
    case T_DGREAT: r->type = R_APPEND; if (r->fd < 0) r->fd = 1; break;
    case T_LESSAND: r->type = R_DUPIN; if (r->fd < 0) r->fd = 0; break;
    case T_GREATAND: r->type = R_DUPOUT; if (r->fd < 0) r->fd = 1; break;
    case T_LESSGREAT: r->type = R_RW; if (r->fd < 0) r->fd = 0; break;
    default: r->type = R_HERE; if (r->fd < 0) r->fd = 0; break;
    }
  if (peek (p) != T_WORD) syntax (p, "a file name is missing after a redirection");
  r->target = strdup (p->word);
  next (p);
  if (r->type == R_HERE)
    {
      struct heredoc_wait *h = calloc (1, sizeof *h);
      const char *w = r->target;
      str d; sb_init (&d);
      for (; *w; w++) { if (*w == '\'' || *w == '"' || *w == '\\') r->quoted = 1; else sb_addc (&d, *w); }
      h->r = r; h->delim = sb_take (&d); h->strip = (t == T_DLESSDASH);
      *p->pend_tail = h; p->pend_tail = &h->next;
    }
  return r;
}

static int assignment_word (const char *w)
{
  /* name=  name+=  name[expr]=  name[expr]+= */
  const char *e = w;
  size_t n;
  char name[256];
  if (!(isalpha ((unsigned char) *e) || *e == '_')) return 0;
  while (isalnum ((unsigned char) *e) || *e == '_') e++;
  n = (size_t) (e - w);
  if (n >= sizeof name) return 0;
  memcpy (name, w, n); name[n] = 0;
  if (*e == '[') { int d = 0; for (; *e; e++) { if (*e == '[') d++; else if (*e == ']' && --d == 0) { e++; break; } } if (d) return 0; }
  if (*e == '+') e++;
  return *e == '=';
}

static const char *find_alias (const char *name);
static struct node *parse_simple (struct parser *p)
{
  struct node *n = new_node (N_SIMPLE);
  peek (p);
  n->line = (int) p->tok_line;
  struct redir **tail = &n->redirs;
  int seen_word = 0;
  for (;;)
    {
      int t = peek (p);
      if (t == T_IONUM || is_redir_tok (t)) { *tail = parse_redir (p); tail = &(*tail)->next; continue; }
      if (t != T_WORD) break;
      if (!seen_word && !assignment_word (p->word) && strspn (p->word, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_!%,@-.") == strlen (p->word))
	{
	  const char *val = find_alias (p->word);
	  if (val && !(p->no_alias && !strcmp (p->no_alias, p->word)))
	    {
	      /* replace the word by the text of the alias; the word is not looked at again in this command */
	      size_t rest = p->len - p->pos;
	      char *nb = malloc (strlen (val) + rest + 1);
	      strcpy (nb, val);
	      memcpy (nb + strlen (val), p->s + p->pos, rest);
	      nb[strlen (val) + rest] = 0;
	      free (p->no_alias); p->no_alias = strdup (p->word);
	      p->s = nb; p->pos = 0; p->len = strlen (nb); p->peeked = 0;
	      continue;
	    }
	}
      if (!seen_word && assignment_word (p->word)) l_add (&n->assigns, strdup (p->word));
      else { seen_word = 1; l_add (&n->words, strdup (p->word)); }
      next (p);
    }
  free (p->no_alias); p->no_alias = 0;
  return n;
}

static void attach_redirs (struct parser *p, struct node *n)
{
  struct redir **tail = &n->redirs;
  while (*tail) tail = &(*tail)->next;
  while (peek (p) == T_IONUM || is_redir_tok (peek (p))) { *tail = parse_redir (p); tail = &(*tail)->next; }
}

static void expect_kw (struct parser *p, const char *kw)
{
  if (!is_kw (p, kw)) { char m[80]; snprintf (m, sizeof m, "\"%s\" expected", kw); syntax (p, m); }
  next (p);
}

static struct node *parse_command (struct parser *p)
{
  struct node *n;
  int t = peek (p);
  if (t == T_LP)
    {
      next (p);
      n = new_node (N_SUBSHELL);
      n->a = parse_compound_list (p);
      if (peek (p) != T_RP) syntax (p, "\")\" expected");
      next (p);
      attach_redirs (p, n);
      return n;
    }
  if (t == T_WORD && !strcmp (p->word, "[["))
    {
      next (p);
      n = new_node (N_DBRACKET);
      n->line = (int) p->tok_line;
      for (;;)
	{
	  int k = peek (p);
	  if (k == T_EOF) syntax (p, "\"]]\" expected");
	  if (k == T_NL) { next (p); continue; }
	  if (k == T_WORD && !strcmp (p->word, "]]")) { next (p); break; }
	  if (k == T_WORD || k == T_IONUM) l_add (&n->words, strdup (p->word));
	  else if (k == T_AND) l_add (&n->words, strdup ("&&"));
	  else if (k == T_OR) l_add (&n->words, strdup ("||"));
	  else if (k == T_LP) l_add (&n->words, strdup ("("));
	  else if (k == T_RP) l_add (&n->words, strdup (")"));
	  else if (k == T_LESS) l_add (&n->words, strdup ("<"));
	  else if (k == T_GREAT) l_add (&n->words, strdup (">"));
	  else syntax (p, "unexpected token in [[ ]]");
	  next (p);
	}
      attach_redirs (p, n);
      return n;
    }
  if (t == T_WORD)
    {
      const char *w = p->word;
      if (!strcmp (w, "{"))
	{
	  next (p);
	  n = new_node (N_GROUP);
	  n->a = parse_compound_list (p);
	  expect_kw (p, "}");
	  attach_redirs (p, n);
	  return n;
	}
      if (!strcmp (w, "if"))
	{
	  struct node **tail;
	  next (p);
	  n = new_node (N_IF);
	  n->a = parse_compound_list (p);
	  expect_kw (p, "then");
	  n->b = parse_compound_list (p);
	  tail = &n->c;
	  while (is_kw (p, "elif"))
	    {
	      struct node *e = new_node (N_IF);
	      next (p);
	      e->a = parse_compound_list (p);
	      expect_kw (p, "then");
	      e->b = parse_compound_list (p);
	      *tail = e; tail = &e->c;
	    }
	  if (is_kw (p, "else")) { next (p); *tail = parse_compound_list (p); }
	  expect_kw (p, "fi");
	  attach_redirs (p, n);
	  return n;
	}
      if (!strcmp (w, "while") || !strcmp (w, "until"))
	{
	  n = new_node (w[0] == 'w' ? N_WHILE : N_UNTIL);
	  next (p);
	  n->a = parse_compound_list (p);
	  expect_kw (p, "do");
	  n->b = parse_compound_list (p);
	  expect_kw (p, "done");
	  attach_redirs (p, n);
	  return n;
	}
      if (!strcmp (w, "for"))
	{
	  next (p);
	  if (peek (p) != T_WORD || !valid_name (p->word)) syntax (p, "a name is missing after \"for\"");
	  n = new_node (N_FOR);
	  n->name = strdup (p->word);
	  next (p);
	  skip_nl (p);
	  if (is_kw (p, "in"))
	    {
	      next (p);
	      n->has_in = 1;
	      while (peek (p) == T_WORD) { l_add (&n->words, strdup (p->word)); next (p); }
	    }
	  if (peek (p) == T_SEMI || peek (p) == T_NL) next (p);
	  skip_nl (p);
	  expect_kw (p, "do");
	  n->a = parse_compound_list (p);
	  expect_kw (p, "done");
	  attach_redirs (p, n);
	  return n;
	}
      if (!strcmp (w, "case"))
	{
	  struct caseitem **tail;
	  next (p);
	  if (peek (p) != T_WORD) syntax (p, "a word is missing after \"case\"");
	  n = new_node (N_CASE);
	  l_add (&n->words, strdup (p->word));
	  next (p);
	  skip_nl (p);
	  expect_kw (p, "in");
	  skip_nl (p);
	  tail = &n->items;
	  while (!is_kw (p, "esac"))
	    {
	      struct caseitem *it = calloc (1, sizeof *it);
	      if (peek (p) == T_LP) next (p);
	      while (peek (p) == T_WORD) { l_add (&it->pats, strdup (p->word)); next (p); if (peek (p) == T_PIPE) next (p); else break; }
	      if (peek (p) != T_RP) syntax (p, "\")\" expected in a case");
	      next (p);
	      it->body = parse_list (p, 0);					/* (may be empty: pattern) ;;) */
	      *tail = it; tail = &it->next;
	      if (peek (p) == T_DSEMI) { next (p); skip_nl (p); }
	      else if (!is_kw (p, "esac")) syntax (p, "\";;\" or \"esac\" expected");
	    }
	  next (p);
	  attach_redirs (p, n);
	  return n;
	}
    }
  n = parse_simple (p);
  /* name () compound  : a function */
  if (n->words.n == 1 && n->assigns.n == 0 && peek (p) == T_LP)
    {
      struct node *f;
      next (p);
      if (peek (p) != T_RP) syntax (p, "\")\" expected in a function definition");
      next (p);
      skip_nl (p);
      f = new_node (N_FUNC);
      f->name = n->words.v[0];
      f->a = parse_command (p);
      return f;
    }
  if (n->words.n == 0 && n->assigns.n == 0 && !n->redirs) syntax (p, "a command is missing");
  return n;
}

static struct node *parse_pipeline (struct parser *p)
{
  struct node *left;
  int negate = 0;
  if (is_kw (p, "!")) { negate = 1; next (p); }
  left = parse_command (p);
  while (peek (p) == T_PIPE)
    {
      struct node *pn = new_node (N_PIPE);
      next (p);
      skip_nl (p);
      pn->a = left; pn->b = parse_command (p);
      left = pn;
    }
  if (negate) { struct node *nn = new_node (N_NOT); nn->a = left; return nn; }
  return left;
}

static struct node *parse_and_or (struct parser *p)
{
  struct node *left = parse_pipeline (p);
  for (;;)
    {
      int t = peek (p);
      struct node *n;
      if (t != T_AND && t != T_OR) return left;
      next (p);
      skip_nl (p);
      n = new_node (t == T_AND ? N_AND : N_OR);
      n->a = left; n->b = parse_pipeline (p);
      left = n;
    }
}

static int at_list_end (struct parser *p)
{
  int t = peek (p);
  if (t == T_EOF || t == T_RP || t == T_DSEMI) return 1;
  if (t == T_WORD)
    {
      const char *w = p->word;
      return !strcmp (w, "then") || !strcmp (w, "else") || !strcmp (w, "elif") || !strcmp (w, "fi") || !strcmp (w, "do") || !strcmp (w, "done") || !strcmp (w, "esac") || !strcmp (w, "}");
    }
  return 0;
}

/* a list up to a newline (toplevel) or up to a closing word */
static struct node *parse_list (struct parser *p, int toplevel)
{
  struct node *head = 0, **tail = &head;
  for (;;)
    {
      struct node *n, *item;
      int t;
      while (peek (p) == T_NL && !toplevel) next (p);
      if (at_list_end (p) || (toplevel && peek (p) == T_NL)) break;
      n = parse_and_or (p);
      t = peek (p);
      if (t == T_AMP) { struct node *b = new_node (N_BG); b->a = n; n = b; next (p); }
      else if (t == T_SEMI) next (p);
      item = n;
      if (!head) { head = item; }
      else
	{
	  struct node *l = new_node (N_LIST);
	  l->a = head; l->b = item; head = l;
	}
      (void) tail;
      if (toplevel && (peek (p) == T_NL || peek (p) == T_EOF)) break;
      if (t != T_AMP && t != T_SEMI && peek (p) != T_NL)
	{
	  if (at_list_end (p)) break;
	  syntax (p, "unexpected word");
	}
    }
  return head;
}
static struct node *parse_compound_list (struct parser *p)
{
  struct node *n = parse_list (p, 0);
  if (!n) syntax (p, "an empty list");
  return n;
}

/* ------------------------------------------------------------------------------------------------------ output sinks, scrap files */
enum { K_CONSOLE, K_BUF, K_PATH };
struct sink { int kind; str *buf; char *path; int append; FILE *fp; };
static struct sink cur_out = { K_CONSOLE }, cur_err = { K_CONSOLE };
static int in_gen, pos_gen = -1;				/* standard input position shared by the commands of a group */
static long in_pos;
static char *cur_in_path;				/* standard input is this file, or the console */
static FILE *cur_in_fp;
static int err_null;					/* 2>/dev/null */

#define NFD 20
struct fdent { int kind; struct sink out; char *in_path; FILE *in_fp; };	/* kind: 0 not open, 1 output, 2 input */
static struct fdent fdtab[NFD];
static int cur_in_fd = -1;				/* standard input is the descriptor (3 and up) of fdtab: its FILE is kept there */


static unsigned scrap_seq;
static char *scrap_name (void)
{
  char b[128];
  snprintf (b, sizeof b, "%s/sh%u_%u", getenv ("SH_TMPDIR") ? getenv ("SH_TMPDIR") : "/tmp", (unsigned) getpid (), ++scrap_seq);
  return strdup (b);
}
static const char *ros (const char *unixname, char *buf, size_t size)	/* the RISC OS name of a file, for a command */
{
  char t[1024];
  const char *r = __riscos_path (unixname, t, 0);
  snprintf (buf, size, "%s", r ? r : unixname);
  return buf;
}

/* A file that two sinks write (cmd >f 2>&1) is open once: a file cannot be opened twice for writing on RISC OS. */
static struct { char *path; FILE *fp; int refs; } open_files[16];
static void sink_open (struct sink *s)
{
  if (s->kind == K_PATH && !s->fp)
    {
      const char *real = !strcmp (s->path, "/dev/null") ? "/tmp/sh_devnull" : s->path;
      int i, free_slot = -1;
      for (i = 0; i < 16; i++)
	{
	  if (open_files[i].fp && !strcmp (open_files[i].path, real)) { open_files[i].refs++; s->fp = open_files[i].fp; return; }
	  if (!open_files[i].fp && free_slot < 0) free_slot = i;
	}
      s->fp = fopen (real, "a");
      if (!s->fp) { fprintf (stderr, "sh: cannot create %s: %s\n", s->path, strerror (errno)); return; }
      if (free_slot >= 0) { open_files[free_slot].path = strdup (real); open_files[free_slot].fp = s->fp; open_files[free_slot].refs = 1; }
    }
}
static void sink_close (struct sink *s)
{
  int i;
  if (!s->fp) return;
  for (i = 0; i < 16; i++)
    if (open_files[i].fp == s->fp)
      {
	if (--open_files[i].refs > 0) { fflush (s->fp); s->fp = 0; return; }
	free (open_files[i].path); open_files[i].fp = 0;
	break;
      }
  fclose (s->fp);
  s->fp = 0;
}
/* a producer in front of  head -n N  needs to make only N lines: pipe_limit >= 0 while such a stage runs; its output is cut after N lines and the stage is stopped (broken_pipe) as a write to a closed pipe would */
static long pipe_limit = -1, pipe_lines;
static const char *pipe_limit_path;
static int broken_pipe;
static void out_write (struct sink *s, const char *p, size_t n)
{
  if (pipe_limit >= 0 && s->kind == K_PATH && pipe_limit_path && !strcmp (s->path, pipe_limit_path))
    {
      size_t i;
      if (broken_pipe) return;
      for (i = 0; i < n; i++)
	if (p[i] == '\n' && ++pipe_lines >= pipe_limit) { n = i + 1; broken_pipe = 1; break; }
      if (pipe_limit == 0) { broken_pipe = 1; return; }
    }
  if (s->kind == K_BUF) sb_add (s->buf, p, n);
  else if (s->kind == K_PATH) { sink_open (s); if (s->fp) fwrite (p, 1, n, s->fp); }
  else if (s == &cur_err) { if (!err_null) { fflush (stdout); fwrite (p, 1, n, stderr); fflush (stderr); } }
  else fwrite (p, 1, n, stdout);
}
static void outs (const char *s) { out_write (&cur_out, s, strlen (s)); }
static void errs (const char *s) { out_write (&cur_err, s, strlen (s)); }
static void errf (const char *fmt, ...)
{
  char b[2048];
  va_list ap;
  va_start (ap, fmt); vsnprintf (b, sizeof b, fmt, ap); va_end (ap);
  out_write (&cur_err, b, strlen (b));
}
static void out_printf (struct sink *s, const char *fmt, ...)
{
  char b[2048];
  va_list ap;
  va_start (ap, fmt); vsnprintf (b, sizeof b, fmt, ap); va_end (ap);
  out_write (s, b, strlen (b));
}

static int redirect_ok = -1;				/* does OS_CLI take  command { > file } ? */
static int can_redirect (void)
{
  if (redirect_ok < 0)
    {
      char *t = scrap_name (), rb[1024], cmd[1400];
      redirect_ok = 0;
      ros (t, rb, sizeof rb);
      snprintf (cmd, sizeof cmd, "Set Sh$Probe 1 { > %s }", rb);	/* with the redirection the (empty) file is made; without, nothing is written or shown */
      __ul64_system_raw (cmd);
      if (access (t, F_OK) == 0) redirect_ok = 1;
      remove (t); free (t);
    }
  return redirect_ok;
}

/* ------------------------------------------------------------------------------------------------------ running a program */
static int is_exec_file (const char *path)
{
  struct stat st;
  return stat (path, &st) == 0 && S_ISREG (st.st_mode);
}
static char *find_in_path (const char *cmd)
{
  const char *path = get_var ("PATH");
  char buf[1024];
  const char *p;
  if (strchr (cmd, '/')) return is_exec_file (cmd) ? strdup (cmd) : 0;
  if (!path) path = ".:/bin:/usr/bin";
  for (p = path; ; )
    {
      const char *e = strchr (p, ':');
      size_t n = e ? (size_t) (e - p) : strlen (p);
      if (n == 0) snprintf (buf, sizeof buf, "./%s", cmd);
      else snprintf (buf, sizeof buf, "%.*s/%s", (int) n, p, cmd);
      if (is_exec_file (buf)) return strdup (buf);
      if (!e) break;
      p = e + 1;
    }
  return 0;
}

static int program_exists (const char *name) { char *p = find_in_path (name); int r = p != 0; free (p); return r; }

static void quote_arg (str *b, const char *a)
{
  int need = *a == 0 || strpbrk (a, " \t\"\n\r\177") != 0;
  sb_addc (b, ' ');
  if (!need) { sb_adds (b, a); return; }
  int brace = strchr (a, '{') != 0;
  sb_addc (b, '"');
  for (; *a; a++)
    {
      if (*a == '"') { if (brace) { sb_addc (b, 127); sb_addc (b, 'q'); continue; } sb_addc (b, '"'); }
      if (*a == '\n' || *a == '\r' || *a == '\t' || *a == 127) { sb_addc (b, 127); sb_addc (b, *a == '\n' ? 'n' : *a == '\r' ? 'r' : *a == '\t' ? 't' : 127); continue; }
      sb_addc (b, *a);
    }
  sb_addc (b, '"');
}

/* run a RISC OS command line with the output and input as given; the exit status */
static int run_command_line (const char *cmdline, struct sink *out, const char *in_path)
{
  str c; char tmp[1100], rb[1100], *scrap = 0;
  int status, spool = 0, exec_in = 0, direct = 0, use_redirect = can_redirect ();
  const char *outpath = 0;
  int append_after = 0;
  sb_init (&c);
  sb_adds (&c, cmdline);
  fflush (stdout); fflush (stderr);
  if (out->kind == K_BUF || (out->kind == K_PATH && out->append) || (out->kind == K_PATH && !strcmp (out->path, "/dev/null")))
    {
      scrap = scrap_name();
      outpath = scrap;
      append_after = out->kind == K_BUF || out->append;
    }
  else if (out->kind == K_PATH) { outpath = out->path; direct = 1; }
  (void) direct;
  if (outpath)
    {
      ros (outpath, rb, sizeof rb);
      if (use_redirect) { snprintf (tmp, sizeof tmp, " { > %s }", rb); sb_adds (&c, tmp); }
      else { snprintf (tmp, sizeof tmp, "Spool %s", rb); __ul64_system_raw (tmp); spool = 1; }
    }
  if (in_path)
    {
      ros (in_path, rb, sizeof rb);
      if (use_redirect) { snprintf (tmp, sizeof tmp, " { < %s }", rb); sb_adds (&c, tmp); }
      else { snprintf (tmp, sizeof tmp, "Exec %s", rb); __ul64_system_raw (tmp); exec_in = 1; }
    }
  status = __ul64_system_raw (c.s);
  if (exec_in) __ul64_system_raw ("Exec");
  if (spool) __ul64_system_raw ("Spool");
  if (append_after && scrap)
    {
      FILE *f = fopen (scrap, "r");
      if (f)
	{
	  char buf[4096]; size_t n;
	  while ((n = fread (buf, 1, sizeof buf, f)) > 0) out_write (out, buf, n);
	  fclose (f);
	}
    }
  if (scrap) { remove (scrap); free (scrap); }
  sb_free (&c);
  return WIFEXITED (status) ? WEXITSTATUS (status) : 128 + (WIFSIGNALED (status) ? WTERMSIG (status) : 1);
}

static char *self_path;				/* this shell, for running scripts */
/* a file that is text (or starts with #!) is a script for this shell */
static int is_script (const char *path)
{
  FILE *f = fopen (path, "rb");
  unsigned char b[256];
  size_t n, i;
  if (!f) return 0;
  n = fread (b, 1, sizeof b, f);
  fclose (f);
  if (n == 0) return 1;				/* an empty file: a script that does nothing */
  if (n >= 2 && b[0] == '#' && b[1] == '!') return 1;
  for (i = 0; i < n; i++) if (b[i] == 0 || (b[i] < 32 && b[i] != '\n' && b[i] != '\r' && b[i] != '\t' && b[i] != 12)) return 0;
  return 1;
}
/* A name that is not a file in PATH is a command of the operating system only if it is one that RISC OS has (the operating system would otherwise be asked, and it can hang waiting for the keyboard on an
   unknown command), or an Alias$ variable, or starts with *. */
extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);
static int is_ros_command (const char *name)
{
  static const char *const known[] = { "cat", "copy", "delete", "dir", "cdir", "count", "create", "ex", "info", "lcat", "lex", "list", "lib", "back", "rename", "type", "wipe", "access", "stamp", "settype",
    "set", "seteval", "setmacro", "unset", "show", "echo", "fx", "run", "obey", "spool", "exec", "rmload", "rmkill", "rmensure", "rmreinit", "modules", "wimpslot", "help", "time", "desktop", "filer_run", "filer_opendir",
    "configure", "status", "ifthere", "x", "shut", "shutdown", "mount", "dismount", "free", "bye", "up", "opt", "title", "pointer", "iconsprites", "unalias", "alias", "key", "memoryi", "memory", "go", "load", "save", "print",
    "quit", "sync", "screenload", "screensave", "spritefile", "ttyexec", "ttyspool", "ttyspoolon", "ttyspooloff", "close", "cache", "shadow", "tv", "wimpmode", "uptime", 0 };
  char low[64], var[96];
  size_t i;
  unsigned long r[10];
  if (name[0] == '*') return 1;
  for (i = 0; name[i] && i < sizeof low - 1; i++) low[i] = (char) (name[i] >= 'A' && name[i] <= 'Z' ? name[i] + 32 : name[i]);
  low[i] = 0;
  for (i = 0; known[i]; i++) if (!strcmp (low, known[i])) return 1;
  snprintf (var, sizeof var, "Alias$%s", name);
  { char buf[8]; r[0] = (unsigned long) var; r[1] = (unsigned long) buf; r[2] = sizeof buf - 1; r[3] = 0; r[4] = 0; if (!swi_x (0x23, r)) return 1; }
  return 0;
}
static void touch_file (const char *path) { FILE *f = fopen (path, "a"); if (f) fclose (f); }
static char *saved_env (const char *name) { const char *v = getenv (name); return v ? strdup (v) : 0; }
static void restore_env (const char *name, char *old) { if (old) { setenv (name, old, 1); free (old); } else unsetenv (name); }
static int same_sink (struct sink *a, struct sink *b) { return a->kind == b->kind && (a->kind == K_CONSOLE || (a->kind == K_BUF && a->buf == b->buf) || (a->kind == K_PATH && a->path && b->path && !strcmp (a->path, b->path))); }

/* Standard input of a group ( a ; b ) < file is one file position for the commands of the group (as on Unix): a program is told where to start reading (Sh$StdinPos) and says where it stopped (Sh$StdinPosOut). */
static int sys_num (const char *name, long *v)
{
  char b[24];
  unsigned long q[10] = { (unsigned long) name, (unsigned long) b, sizeof b - 1, 0, 3 };
  if (swi_x (0x23, q) || q[2] == 0 || q[2] >= sizeof b) return 0;
  b[q[2]] = 0; *v = atol (b);
  { unsigned long d[10] = { (unsigned long) name, (unsigned long) "", (unsigned long) -1, 0, 4, 0, 0, 0, 0, 0 }; swi_x (0x24, d); }	/* remove it */
  return 1;
}
static int run_external (list *argv, struct sink *out, const char *in_path, struct sink *err)
{
  char *prog = find_in_path (argv->v[0]);
  str c; int i, status;
  char rb[1100];
  sb_init (&c);
  if (!prog)
    {
      if (strchr (argv->v[0], '/') || !is_ros_command (argv->v[0])) { errf ("sh: %s: not found\n", argv->v[0]); return 127; }
      /* a command of the operating system: its screen output is captured with { > } or *Spool, its input is not available */
      sb_adds (&c, argv->v[0]);
      for (i = 1; i < argv->n; i++) quote_arg (&c, argv->v[i]);
      status = run_command_line (c.s, out, 0);
      sb_free (&c);
      return status;
    }
  if (self_path && is_script (prog))
    {
      list sa = { 0, 0, 0 };
      int k, r;
      l_add (&sa, self_path); l_add (&sa, prog);
      for (k = 1; k < argv->n; k++) l_add (&sa, argv->v[k]);
      r = run_external (&sa, out, in_path, err);
      free (sa.v);
      free (prog);
      return r;
    }
  /* a program of the SDK: its standard streams are set with the variables Sh$Stdin, Sh$Stdout, Sh$Stderr (the library of the program reads them) */
  {
    char *o_in = saved_env ("Sh$Stdin"), *o_out = saved_env ("Sh$Stdout"), *o_err = saved_env ("Sh$Stderr");
    char *scrap = 0;
    int track = 0;
    ros (prog, rb, sizeof rb); sb_adds (&c, rb); free (prog);
    for (i = 1; i < argv->n; i++) quote_arg (&c, argv->v[i]);
    if (out->kind == K_PATH)
      {
	if (!strcmp (out->path, "/dev/null")) setenv ("Sh$Stdout", "null", 1);
	else { char t[1100]; touch_file (out->path); ros (out->path, t, sizeof t); setenv ("Sh$Stdout", t, 1); }
      }
    else if (out->kind == K_BUF) { char t[1100]; scrap = scrap_name (); touch_file (scrap); ros (scrap, t, sizeof t); setenv ("Sh$Stdout", t, 1); }
    if (in_path) { char t[1100]; ros (in_path, t, sizeof t); setenv ("Sh$Stdin", t, 1); }
    track = in_path && in_path == cur_in_path;
    if (track)
      {
	char pb[24];
	if (pos_gen != in_gen) { in_pos = cur_in_fp ? ftell (cur_in_fp) : 0; if (in_pos < 0) in_pos = 0; pos_gen = in_gen; }
	snprintf (pb, sizeof pb, "%ld", in_pos);
	setenv ("Sh$StdinPos", pb, 1);
      }
    if (pipe_limit >= 0 && out->kind == K_PATH && pipe_limit_path && !strcmp (out->path, pipe_limit_path)) { char t[24]; snprintf (t, sizeof t, "%ld", pipe_limit); setenv ("Sh$StdoutLines", t, 1); }
    if (err_null) setenv ("Sh$Stderr", "null", 1);
    else if (err->kind == K_PATH && !same_sink (err, out))
      {
	if (!strcmp (err->path, "/dev/null")) setenv ("Sh$Stderr", "null", 1);
	else { char t[1100]; touch_file (err->path); ros (err->path, t, sizeof t); setenv ("Sh$Stderr", t, 1); }
      }
    else if (err->kind != K_CONSOLE && same_sink (err, out)) setenv ("Sh$Stderr", "stdout", 1);
    fflush (stdout); fflush (stderr);
    setenv ("Sh$Argv0", argv->v[0], 1);				/* the program shows this name, not its full path */
    status = __ul64_system_raw (c.s);
    if (track)
      {
	long np;
	unsetenv ("Sh$StdinPos");
	if (sys_num ("Sh$StdinPosOut", &np)) { in_pos = np; if (cur_in_fp) fseek (cur_in_fp, np, SEEK_SET); }
      }
    unsetenv ("Sh$Argv0");
    restore_env ("Sh$Stdin", o_in); restore_env ("Sh$Stdout", o_out); restore_env ("Sh$Stderr", o_err);
    unsetenv ("Sh$StdoutLines");
    if (scrap)
      {
	FILE *f = fopen (scrap, "r");
	if (f) { char buf[4096]; size_t n; while ((n = fread (buf, 1, sizeof buf, f)) > 0) out_write (out, buf, n); fclose (f); }
	remove (scrap); free (scrap);
      }
  }
  sb_free (&c);
  return WIFEXITED (status) ? WEXITSTATUS (status) : 128 + (WIFSIGNALED (status) ? WTERMSIG (status) : 1);
}

/* ------------------------------------------------------------------------------------------------------ expansion */
static int profile;
static int exec_node (struct node *n);
static int exec_string (const char *text, const char *name);
static char *capture (const char *text);
static long arith (const char *expr);

struct field { str s; str mask; int started; };	/* mask: 'q' for a character that is quoted (not a pattern, not split) */
static void f_init (struct field *f) { sb_init (&f->s); sb_init (&f->mask); f->started = 0; }
static void f_add (struct field *f, const char *p, size_t n, int quoted)
{
  size_t i;
  for (i = 0; i < n; i++) { sb_addc (&f->s, p[i]); sb_addc (&f->mask, quoted ? 'q' : 'u'); }
  f->started = 1;
}
static const char *ifs_chars (void) { const char *i = get_var ("IFS"); return i ? i : " \t\n"; }

struct xctx { list *out; struct field cur; int split; };	/* split: make fields (a command word) or not */

static void emit_field (struct xctx *x)
{
  if (!x->cur.started) return;
  if (!x->split) { f_init (&x->cur); return; }
  {
    int glob_it = 0;
    size_t i;
    if (!opt_f) for (i = 0; i < x->cur.s.n; i++) if (x->cur.mask.s[i] == 'u' && strchr ("*?[", x->cur.s.s[i])) { glob_it = 1; break; }
    if (glob_it)
      {
	str pat; glob_t g; size_t k;
	sb_init (&pat);
	for (k = 0; k < x->cur.s.n; k++)
	  {
	    char c = x->cur.s.s[k];
	    if (x->cur.mask.s[k] == 'q' && strchr ("*?[]\\", c)) sb_addc (&pat, '\\');
	    sb_addc (&pat, c);
	  }
	memset (&g, 0, sizeof g);
	if (glob (pat.s ? pat.s : "", 0, 0, &g) == 0 && g.gl_pathc > 0)
	  {
	    for (k = 0; k < g.gl_pathc; k++) l_add (x->out, strdup (g.gl_pathv[k]));
	    globfree (&g);
	    sb_free (&pat); sb_free (&x->cur.s); sb_free (&x->cur.mask); f_init (&x->cur);
	    return;
	  }
	globfree (&g);
	sb_free (&pat);
      }
    l_add (x->out, sb_take (&x->cur.s));
    sb_free (&x->cur.mask);
    f_init (&x->cur);
  }
}

/* add the text of an expansion: quoted (kept whole) or not (split at IFS) */
static void add_expansion (struct xctx *x, const char *p, size_t n, int quoted)
{
  if (quoted || !x->split) { f_add (&x->cur, p, n, quoted); return; }
  {
    const char *ifs = ifs_chars ();
    size_t i;
    for (i = 0; i < n; i++)
      {
	char c = p[i];
	if (strchr (ifs, c))
	  {
	    if (c == ' ' || c == '\t' || c == '\n') { if (x->cur.started) emit_field (x); }
	    else { x->cur.started = 1; emit_field (x); }
	  }
	else f_add (&x->cur, &c, 1, 0);
      }
  }
}

static const char *special_param (char c, char *buf, size_t size)
{
  switch (c)
    {
    case '?': snprintf (buf, size, "%d", last_status); return buf;
    case '$': snprintf (buf, size, "%d", (int) getpid ()); return buf;
    case '#': snprintf (buf, size, "%d", npos); return buf;
    case '!': snprintf (buf, size, "%d", bg_pid); return buf;
    case '-': buf[0] = 0; if (opt_e) strcat (buf, "e"); if (opt_x) strcat (buf, "x"); if (opt_u) strcat (buf, "u"); return buf;
    case '0': return arg0;
    default: return 0;
    }
}

static char *expand_str (const char *raw);		/* no splitting, no pattern matching: assignments, redirections */
static char *expand_pattern (const char *raw);
static int do_assign (const char *raw);

/* the value of a parameter reference name; *set tells if it is set */
static const char *param_value (const char *name, int *set, char *buf, size_t size)
{
  *set = 1;
  if (isdigit ((unsigned char) name[0]) && name[0] != '0') { int i = atoi (name); if (i >= 1 && i <= npos) return pos_params[i - 1]; *set = 0; return 0; }
  if (name[1] == 0 && strchr ("?$#!-0", name[0])) return special_param (name[0], buf, size);
  if (!strcmp (name, "*") || !strcmp (name, "@"))
    {
      str b; int i;
      const char *ifs_ = get_var ("IFS");
      char sepbuf[2] = { ifs_ ? ifs_[0] : ' ', 0 };
      const char *sep = ifs_ && !ifs_[0] ? "" : sepbuf;
      sb_init (&b);
      sb_adds (&b, "");
      for (i = 0; i < npos; i++) { if (i) sb_adds (&b, sep); sb_adds (&b, pos_params[i]); }
      snprintf (buf, size, "%.*s", (int) (size - 1), b.s);
      *set = npos > 0;
      sb_free (&b);
      return buf;
    }
  {
    const char *v = get_var (name);
    if (!v) { *set = 0; if (opt_u) { errf ("sh: %s: parameter not set\n", name); exit (2); } }
    return v;
  }
}

/* ${#x} ${x#p} ... pattern removal */
static char *trim_pattern (const char *value, const char *pat, int prefix, int longest)
{
  size_t n = strlen (value), i;
  if (prefix)
    {
      for (i = longest ? n + 1 : 0; longest ? i-- > 0 : i <= n; longest ? 0 : i++)
	{
	  char *t = strndup (value, i);
	  int m = fnmatch (pat, t, 0) == 0;
	  free (t);
	  if (m) return strdup (value + i);
	}
    }
  else
    {
      for (i = longest ? 0 : n + 1; longest ? i <= n : i-- > 0; longest ? i++ : 0)
	{
	  if (fnmatch (pat, value + i, 0) == 0) return strndup (value, i);
	}
    }
  return strdup (value);
}

static void expand_into (struct xctx *x, const char *raw, int in_dq);

/* ${v/pat/rep} ${v//pat/rep} ${v/#pat/rep} ${v/%pat/rep}: the longest match of the pattern is replaced (R: the first one, G: all, A: only at the start, E: only at the end) */
static void replace_pattern (str *res, const char *v, const char *pat, const char *rep, int mode)
{
  size_t vl = strlen (v), i = 0;
  while (i <= vl)
    {
      size_t e = 0;
      int found = 0;
      if (*pat && (mode != 'A' || i == 0) && i < vl)
	for (e = (mode == 'E') ? vl : vl; e > i; e--)
	  {
	    char *t;
	    if (mode == 'E' && e != vl) break;
	    t = strndup (v + i, e - i);
	    found = fnmatch (pat, t, 0) == 0;
	    free (t);
	    if (found) break;
	  }
      if (found)
	{
	  sb_adds (res, rep);
	  i = e;
	  if (mode != 'G') { sb_adds (res, v + i); return; }
	  continue;
	}
      if (mode == 'A' || mode == 'E') { if (mode == 'A') { sb_adds (res, v); return; } }
      if (i < vl) sb_addc (res, v[i]);
      i++;
    }
}

static void expand_dollar (struct xctx *x, const char **pp, int in_dq)
{
  const char *p = *pp;			/* just after the $ */
  char buf[64];
  int set;
  if (*p == '(' && p[1] == '(')
    {
      int depth = 0; const char *q = p;
      char *inner, *e; long v; char num[32];
      for (; *q; q++) { if (*q == '(') depth++; else if (*q == ')') { if (--depth == 0) break; } }
      inner = strndup (p + 2, (size_t) (q - p) - 3);
      e = expand_str (inner);
      v = arith (e);
      snprintf (num, sizeof num, "%ld", v);
      add_expansion (x, num, strlen (num), in_dq);
      free (inner); free (e);
      *pp = q + 1;
      return;
    }
  if (*p == '(')
    {
      int depth = 0; const char *q = p; char *inner, *out;
      for (; *q; q++)
	{
	  if (*q == '(') depth++;
	  else if (*q == ')') { if (--depth == 0) break; }
	  else if (*q == '\'') { for (q++; *q && *q != '\''; q++) ; }
	  else if (*q == '"') { for (q++; *q && *q != '"'; q++) if (*q == '\\' && q[1]) q++; }
	  else if (*q == '\\' && q[1]) q++;
	}
      inner = strndup (p + 1, (size_t) (q - p) - 1);
      out = capture (inner);
      add_expansion (x, out, strlen (out), in_dq);
      free (inner); free (out);
      *pp = *q ? q + 1 : q;
      return;
    }
  if (*p == '{')
    {
      int depth = 0; const char *q = p; char *inner;
      char name[256]; size_t nl = 0;
      const char *val; int colon = 0; char op = 0; const char *word = 0;
      for (; *q; q++) { if (*q == '{') depth++; else if (*q == '}') { if (--depth == 0) break; } else if (*q == '\\' && q[1]) q++; }
      inner = strndup (p + 1, (size_t) (q - p) - 1);
      *pp = *q ? q + 1 : q;
      {
	/* ${a[i]} ${a[@]} ${a[*]} ${#a[@]} ${#a[i]} ${!a[@]} ${a[@]:off:len} */
	const char *q = inner;
	int len_of = 0, keys = 0;
	char an[256]; size_t k = 0;
	if (*q == '#') { len_of = 1; q++; } else if (*q == '!') { keys = 1; q++; }
	while ((isalnum ((unsigned char) *q) || *q == '_') && k < sizeof an - 1) an[k++] = *q++;
	an[k] = 0;
	if (k && *q == '[' && (isalpha ((unsigned char) an[0]) || an[0] == '_'))
	  {
	    const char *cl = q; int d = 0;
	    for (; *cl; cl++) { if (*cl == '[') d++; else if (*cl == ']' && --d == 0) break; }
	    if (*cl)
	      {
		char *ix = strndup (q + 1, (size_t) (cl - q - 1));
		const char *rest = cl + 1;
		struct arr *a = find_arr (an);
		int all = !strcmp (ix, "@") || !strcmp (ix, "*");
		int at = !strcmp (ix, "@"), i;
		if (!a && get_var (an)) a = make_arr (an);
		if (len_of && all) { char num[32]; snprintf (num, sizeof num, "%d", a ? arr_count (a) : 0); add_expansion (x, num, strlen (num), in_dq); }
		else if (len_of) { char *ex = expand_str (ix), num[32]; long i2 = arith (ex); snprintf (num, sizeof num, "%zu", a && i2 >= 0 && i2 < a->n && a->v[i2] ? strlen (a->v[i2]) : 0); add_expansion (x, num, strlen (num), in_dq); free (ex); }
		else if (keys)
		  {
		    int first = 1;
		    for (i = 0; a && i < a->n; i++) if (a->v[i])
		      { char num[32]; snprintf (num, sizeof num, "%d", i); if (at && in_dq) { if (!first) emit_field (x); f_add (&x->cur, num, strlen (num), 1); } else { if (!first) add_expansion (x, " ", 1, in_dq); add_expansion (x, num, strlen (num), in_dq); } first = 0; }
		  }
		else if (all)
		  {
		    long off = 0, cnt = -1, seen = 0;
		    int first = 1;
		    if (*rest == ':') { char *r2 = expand_str (rest + 1), *c2 = strchr (r2, ':'); if (c2) *c2++ = 0; off = arith (r2); if (c2) cnt = arith (c2); free (r2); }
		    if (off < 0) { off += a ? arr_count (a) : 0; if (off < 0) off = 0; }
		    for (i = 0; a && i < a->n; i++) if (a->v[i])
		      {
			if (seen++ < off) continue;
			if (cnt >= 0 && cnt-- == 0) break;
			if (at && in_dq) { if (!first) emit_field (x); f_add (&x->cur, a->v[i], strlen (a->v[i]), 1); }
			else { const char *ifs_ = get_var ("IFS"); char sp[2] = { in_dq && !at && ifs_ ? ifs_[0] : ' ', 0 }; if (!first) add_expansion (x, sp, 1, in_dq); add_expansion (x, a->v[i], strlen (a->v[i]), in_dq); }
			first = 0;
		      }
		    if (first && at && in_dq) x->cur.started = x->cur.started;
		  }
		else
		  {
		    char *ex = expand_str (ix);
		    long i2 = arith (ex);
		    if (a && i2 < 0) i2 += a->n;
		    const char *v = a && i2 >= 0 && i2 < a->n ? a->v[i2] : 0;
		    if (!v && !a && i2 == 0) v = get_var (an);
		    if (!v && rest[0] == ':' && rest[1] == '-') { char *w2 = expand_str (rest + 2); add_expansion (x, w2, strlen (w2), in_dq); free (w2); }
		    else if (!v && rest[0] == '-') { char *w2 = expand_str (rest + 1); add_expansion (x, w2, strlen (w2), in_dq); free (w2); }
		    else if (v) add_expansion (x, v, strlen (v), in_dq);
		    free (ex);
		  }
		free (ix); free (inner);
		return;
	      }
	  }
      }
      if (inner[0] == '#' && inner[1])		/* ${#x} */
	{
	  char num[32];
	  val = param_value (inner + 1, &set, buf, sizeof buf);
	  if (!strcmp (inner + 1, "@") || !strcmp (inner + 1, "*")) snprintf (num, sizeof num, "%d", npos);
	  else snprintf (num, sizeof num, "%zu", val ? strlen (val) : 0);
	  add_expansion (x, num, strlen (num), in_dq);
	  free (inner);
	  return;
	}
      {
	const char *s = inner;
	if (strchr ("?$#!-0123456789@*", *s) && !isalpha ((unsigned char) *s)) { while (*s && (isdigit ((unsigned char) *s) && isdigit ((unsigned char) inner[0]) ? 1 : nl == 0) && nl < sizeof name - 1) { name[nl++] = *s++; if (!isdigit ((unsigned char) inner[0])) break; } }
	else while ((isalnum ((unsigned char) *s) || *s == '_') && nl < sizeof name - 1) name[nl++] = *s++;
	name[nl] = 0;
	if (*s == '/' ) { op = 'R'; s++; if (*s == '/') { op = 'G'; s++; } else if (*s == '#') { op = 'A'; s++; } else if (*s == '%') { op = 'E'; s++; } word = s; }
	else if (*s == '^' || *s == ',') { op = *s++; if (*s == op) { op = (char) (op == '^' ? 'U' : 'L'); s++; } word = s; }
	else if (*s == ':' && s[1] && !strchr ("-=?+", s[1])) { op = 'O'; s++; word = s; }
	else {
	if (*s == ':') { colon = 1; s++; }
	if (*s) { op = *s++; if ((op == '#' || op == '%') && *s == op) { op = (char) (op == '#' ? 'P' : 'S'); s++; } word = s; }
	}
      }
      if (!strcmp (name, "@") && in_dq) { /* "${@}" like "$@" */ }
      val = param_value (name, &set, buf, sizeof buf);
      {
	int use_default = op && strchr ("-=?+", op) ? (colon ? (!set || !val || !*val) : !set) : 0;
	char *w = word ? expand_str (word) : 0;
	if (op == '-' && use_default) add_expansion (x, w, strlen (w), in_dq);
	else if (op == '=' && use_default) { set_var (name, w); add_expansion (x, w, strlen (w), in_dq); }
	else if (op == '?' && use_default) { errf ("sh: %s: %s\n", name, w && *w ? w : "parameter null or not set"); exit (2); }
	else if (op == '+') { if (colon ? (set && val && *val) : set) add_expansion (x, w, strlen (w), in_dq); }
	else if (op == 'R' || op == 'G' || op == 'A' || op == 'E')
	  {
	    /* ${v/pat/rep} ${v//pat/rep} ${v/#pat/rep} ${v/%pat/rep}: the longest match of the pattern is replaced */
	    const char *v = val ? val : "";
	    const char *sl = word ? word : "";
	    const char *q2;
	    char *patraw, *pat, *rep;
	    str res;
	    for (q2 = sl; *q2 && *q2 != '/'; q2++) if (*q2 == '\\' && q2[1]) q2++;
	    patraw = strndup (sl, (size_t) (q2 - sl));
	    pat = expand_pattern (patraw);
	    rep = expand_str (*q2 ? q2 + 1 : "");
	    sb_init (&res); sb_adds (&res, "");
	    replace_pattern (&res, v, pat, rep, op);
	    add_expansion (x, res.s ? res.s : "", res.s ? strlen (res.s) : 0, in_dq);
	    sb_free (&res); free (patraw); free (pat); free (rep);
	  }
	else if (op == 'O')
	  {
	    /* ${v:off} ${v:off:len} */
	    const char *v = val ? val : "";
	    long vl = (long) strlen (v), off, len;
	    const char *colon2 = word ? strchr (word, ':') : 0;
	    char *offs = colon2 ? strndup (word, (size_t) (colon2 - word)) : strdup (word ? word : "0"), *e1 = expand_str (offs), *e2 = 0;
	    off = arith (e1);
	    if (off < 0) { off += vl; if (off < 0) off = 0; }
	    if (off > vl) off = vl;
	    len = vl - off;
	    if (colon2) { e2 = expand_str (colon2 + 1); len = arith (e2); if (len < 0) { len = vl - off + len; if (len < 0) len = 0; } if (len > vl - off) len = vl - off; }
	    add_expansion (x, v + off, (size_t) len, in_dq);
	    free (offs); free (e1); free (e2);
	  }
	else if (op == 'U' || op == 'L' || op == '^' || op == ',')
	  {
	    char *t = strdup (val ? val : "");
	    size_t i;
	    for (i = 0; t[i]; i++)
	      {
		if ((op == '^' || op == ',') && i > 0) break;
		t[i] = (char) ((op == 'U' || op == '^') ? toupper ((unsigned char) t[i]) : tolower ((unsigned char) t[i]));
	      }
	    add_expansion (x, t, strlen (t), in_dq);
	    free (t);
	  }
	else if (op == '#' || op == 'P' || op == '%' || op == 'S')
	  {
	    char *t = trim_pattern (val ? val : "", w, op == '#' || op == 'P', op == 'P' || op == 'S');
	    add_expansion (x, t, strlen (t), in_dq);
	    free (t);
	  }
	else if (!op && (!strcmp (name, "@")) && in_dq)
	  {
	    int i;
	    for (i = 0; i < npos; i++) { if (i) emit_field (x); f_add (&x->cur, pos_params[i], strlen (pos_params[i]), 1); }
	  }
	else if (val && !(op && use_default)) add_expansion (x, val, strlen (val), in_dq);
	free (w);
      }
      free (inner);
      return;
    }
  /* $name, $1, $?, $@ ... */
  {
    char name[256]; size_t nl = 0; const char *val;
    if (isalpha ((unsigned char) *p) || *p == '_') { while ((isalnum ((unsigned char) *p) || *p == '_') && nl < sizeof name - 1) name[nl++] = *p++; }
    else if (isdigit ((unsigned char) *p) || strchr ("?$#!-@*", *p)) name[nl++] = *p++;
    else { f_add (&x->cur, "$", 1, in_dq); *pp = p; return; }
    name[nl] = 0;
    *pp = p;
    if (!strcmp (name, "@") && in_dq)
      {
	int i;
	if (npos == 0) { x->cur.started = x->cur.started; return; }
	for (i = 0; i < npos; i++) { if (i) emit_field (x); f_add (&x->cur, pos_params[i], strlen (pos_params[i]), 1); }
	return;
      }
    if (!strcmp (name, "@"))
      {
	int i;
	for (i = 0; i < npos; i++) add_expansion (x, pos_params[i], strlen (pos_params[i]), 0), add_expansion (x, " ", 1, 0);
	return;
      }
    val = param_value (name, &set, buf, sizeof buf);
    if (val) add_expansion (x, val, strlen (val), in_dq);
  }
}

static void expand_into (struct xctx *x, const char *raw, int in_dq)
{
  const char *p = raw;
  if (!in_dq && *p == '~' && (p[1] == 0 || p[1] == '/'))
    {
      const char *h = get_var ("HOME");
      if (h) { f_add (&x->cur, h, strlen (h), 1); p++; }
    }
  while (*p)
    {
      char c = *p;
      if (c == '\\')
	{
	  if (in_dq && p[1] && !strchr ("$`\"\\\n", p[1])) { f_add (&x->cur, p, 1, 1); p++; }	/* inside "..." a backslash before another character stays */
	  else if (p[1] == '\n' && in_dq) p += 2;
	  else if (p[1]) { f_add (&x->cur, p + 1, 1, 1); p += 2; } else { p++; }
	}
      else if (c == '\'' && !in_dq)
	{
	  const char *e = strchr (p + 1, '\'');
	  size_t n = e ? (size_t) (e - p - 1) : strlen (p + 1);
	  f_add (&x->cur, p + 1, n, 1);
	  x->cur.started = 1;
	  p += n + 2;
	}
      else if (c == '"' && !in_dq)
	{
	  const char *q = p + 1, *e = q;
	  char *inner;
	  while (*e && *e != '"') { if (*e == '\\' && e[1]) e++; else if (*e == '$' && (e[1] == '(' || e[1] == '{')) { int depth = 0; char open = e[1], close = open == '(' ? ')' : '}'; for (e++; *e; e++) { if (*e == open) depth++; else if (*e == close) { if (--depth == 0) break; } } if (!*e) break; } else if (*e == '`') { for (e++; *e && *e != '`'; e++) if (*e == '\\' && e[1]) e++; if (!*e) break; } e++; }
	  inner = strndup (q, (size_t) (e - q));
	  x->cur.started = 1;
	  expand_into (x, inner, 1);
	  free (inner);
	  p = *e ? e + 1 : e;
	}
      else if (c == '$' && p[1] == '\'' && !in_dq)
	{
	  const char *q = p + 2;
	  x->cur.started = 1;
	  while (*q && *q != '\'')
	    {
	      char ch = *q++;
	      if (ch == '\\' && *q)
		{
		  char e = *q++;
		  switch (e)
		    {
		    case 'n': ch = '\n'; break; case 't': ch = '\t'; break; case 'r': ch = '\r'; break; case 'a': ch = '\a'; break;
		    case 'b': ch = '\b'; break; case 'f': ch = '\f'; break; case 'v': ch = '\v'; break; case 'e': case 'E': ch = 27; break;
		    case '\\': case '\'': case '"': case '?': ch = e; break;
		    case 'x': { int v = 0, k = 0; while (k < 2 && isxdigit ((unsigned char) *q)) { v = v * 16 + (isdigit ((unsigned char) *q) ? *q - '0' : (tolower ((unsigned char) *q) - 'a' + 10)); q++; k++; } ch = (char) v; break; }
		    case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7':
		      { int v = e - '0', k = 1; while (k < 3 && *q >= '0' && *q <= '7') { v = v * 8 + (*q - '0'); q++; k++; } ch = (char) v; break; }
		    default: f_add (&x->cur, "\\", 1, 1); ch = e; break;
		    }
		}
	      f_add (&x->cur, &ch, 1, 1);
	    }
	  p = *q ? q + 1 : q;
	}
      else if (c == '$') { const char *q = p + 1; expand_dollar (x, &q, in_dq); p = q; }
      else if (c == '`')
	{
	  const char *e = p + 1;
	  str inner; char *out;
	  sb_init (&inner); sb_adds (&inner, "");
	  while (*e && *e != '`') { if (*e == '\\' && (e[1] == '`' || e[1] == '$' || e[1] == '\\')) e++; sb_addc (&inner, *e); e++; }
	  out = capture (inner.s);
	  add_expansion (x, out, strlen (out), in_dq);
	  free (out); sb_free (&inner);
	  p = *e ? e + 1 : e;
	}
      else if (in_dq && c == '\\') p++;
      else { f_add (&x->cur, &c, 1, in_dq); p++; }
    }
}

/* the fields of a word (a command word, a "for" list) */
static void expand_word (const char *raw, list *out)
{
  struct xctx x;
  x.out = out; x.split = 1; f_init (&x.cur);
  expand_into (&x, raw, 0);
  emit_field (&x);
  sb_free (&x.cur.s); sb_free (&x.cur.mask);
}
static char *expand_str (const char *raw)
{
  struct xctx x; list l = { 0, 0, 0 };
  x.out = &l; x.split = 0; f_init (&x.cur);
  expand_into (&x, raw, 0);
  sb_free (&x.cur.mask);
  return sb_take (&x.cur.s);
}
/* a pattern (case, ${x#p}): quoted characters are escaped */
static char *expand_pattern (const char *raw)
{
  struct xctx x; list l = { 0, 0, 0 };
  str pat; size_t k;
  x.out = &l; x.split = 0; f_init (&x.cur);
  expand_into (&x, raw, 0);
  sb_init (&pat); sb_adds (&pat, "");
  for (k = 0; k < x.cur.s.n; k++)
    {
      if (x.cur.mask.s[k] == 'q' && strchr ("*?[]\\", x.cur.s.s[k])) sb_addc (&pat, '\\');
      sb_addc (&pat, x.cur.s.s[k]);
    }
  sb_free (&x.cur.s); sb_free (&x.cur.mask);
  return sb_take (&pat);
}

/* ------------------------------------------------------------------------------------------------------ arithmetic */
struct ap { const char *s; };
static void ap_ws (struct ap *a) { while (*a->s == ' ' || *a->s == '\t' || *a->s == '\n') a->s++; }
static long ap_expr (struct ap *a, int prec);
static long ap_primary (struct ap *a)
{
  long v;
  ap_ws (a);
  if (*a->s == '(') { a->s++; v = ap_expr (a, 0); ap_ws (a); if (*a->s == ')') a->s++; return v; }
  if (*a->s == '-') { a->s++; return -ap_primary (a); }
  if (*a->s == '+') { a->s++; return ap_primary (a); }
  if (*a->s == '!') { a->s++; return !ap_primary (a); }
  if (*a->s == '~') { a->s++; return ~ap_primary (a); }
  if (isdigit ((unsigned char) *a->s)) { char *e; v = strtol (a->s, &e, 0); a->s = e; return v; }
  if (isalpha ((unsigned char) *a->s) || *a->s == '_')
    {
      char name[128]; size_t n = 0; const char *val;
      while ((isalnum ((unsigned char) *a->s) || *a->s == '_') && n < sizeof name - 1) name[n++] = *a->s++;
      name[n] = 0;
      ap_ws (a);
      if (*a->s == '=' && a->s[1] != '=')
	{
	  char num[32];
	  a->s++;
	  v = ap_expr (a, 1);
	  snprintf (num, sizeof num, "%ld", v);
	  set_var (name, num);
	  return v;
	}
      val = get_var (name);
      return val ? strtol (val, 0, 0) : 0;
    }
  return 0;
}
static int ap_binprec (struct ap *a, int *len)
{
  const char *s = a->s;
  *len = 1;
  if (s[0] == '|' && s[1] == '|') { *len = 2; return 1; }
  if (s[0] == '&' && s[1] == '&') { *len = 2; return 2; }
  if (s[0] == '|') return 3;
  if (s[0] == '^') return 4;
  if (s[0] == '&') return 5;
  if ((s[0] == '=' || s[0] == '!') && s[1] == '=') { *len = 2; return 6; }
  if (s[0] == '<' && s[1] == '<') { *len = 2; return 8; }
  if (s[0] == '>' && s[1] == '>') { *len = 2; return 8; }
  if (s[0] == '<' || s[0] == '>') { if (s[1] == '=') *len = 2; return 7; }
  if (s[0] == '+' || s[0] == '-') return 9;
  if (s[0] == '*' || s[0] == '/' || s[0] == '%') return 10;
  return 0;
}
static long ap_expr (struct ap *a, int minprec)
{
  long l = ap_primary (a);
  for (;;)
    {
      int len, pr;
      long r;
      const char *op;
      ap_ws (a);
      if (*a->s == '?' && minprec <= 0)
	{
	  long t, f;
	  a->s++; t = ap_expr (a, 0); ap_ws (a); if (*a->s == ':') a->s++; f = ap_expr (a, 0);
	  return l ? t : f;
	}
      pr = ap_binprec (a, &len);
      if (!pr || pr < minprec) return l;
      op = a->s; a->s += len;
      r = ap_expr (a, pr + 1);
      if (!strncmp (op, "||", 2)) l = l || r;
      else if (!strncmp (op, "&&", 2)) l = l && r;
      else if (op[0] == '|') l = l | r;
      else if (op[0] == '^') l = l ^ r;
      else if (op[0] == '&') l = l & r;
      else if (!strncmp (op, "==", 2)) l = l == r;
      else if (!strncmp (op, "!=", 2)) l = l != r;
      else if (!strncmp (op, "<<", 2)) l = l << r;
      else if (!strncmp (op, ">>", 2)) l = l >> r;
      else if (!strncmp (op, "<=", 2)) l = l <= r;
      else if (!strncmp (op, ">=", 2)) l = l >= r;
      else if (op[0] == '<') l = l < r;
      else if (op[0] == '>') l = l > r;
      else if (op[0] == '+') l = l + r;
      else if (op[0] == '-') l = l - r;
      else if (op[0] == '*') l = l * r;
      else if (op[0] == '/') { if (!r) die ("division by zero"); l = l / r; }
      else if (op[0] == '%') { if (!r) die ("division by zero"); l = l % r; }
    }
}
static long arith (const char *expr) { struct ap a; a.s = expr; return ap_expr (&a, 0); }

/* ------------------------------------------------------------------------------------------------------ control flow state */
static int brk_n, cont, ret_flag, exit_flag, exit_code;
static int in_condition;			/* set -e does not apply */
static struct func *find_func (const char *name) { struct func *f; for (f = funcs; f; f = f->next) if (!strcmp (f->name, name)) return f; return 0; }
static int func_depth;
struct localsave { char *name; char *old; int had; struct localsave *next; };
static struct localsave *locals;

static char *trap_exit;

/* ------------------------------------------------------------------------------------------------------ builtins */
typedef int (*builtin_fn) (int argc, char **argv);
static int bi_echo (int argc, char **argv)
{
  int i, nl = 1, first = 1;
  str b; sb_init (&b); sb_adds (&b, "");
  i = 1;
  if (i < argc && !strcmp (argv[i], "-n")) { nl = 0; i++; }
  for (; i < argc; i++)
    {
      const char *p = argv[i];
      if (!first) sb_addc (&b, ' ');
      first = 0;
      for (; *p; p++)
	{
	  if (*p == '\\' && p[1])
	    {
	      p++;
	      switch (*p)
		{
		case 'n': sb_addc (&b, '\n'); break;
		case 't': sb_addc (&b, '\t'); break;
		case 'r': sb_addc (&b, '\r'); break;
		case 'a': sb_addc (&b, '\a'); break;
		case 'b': sb_addc (&b, '\b'); break;
		case 'f': sb_addc (&b, '\f'); break;
		case 'v': sb_addc (&b, '\v'); break;
		case '\\': sb_addc (&b, '\\'); break;
		case 'c': nl = 0; goto done;
		case '0': { int v = 0, k; for (k = 0; k < 3 && p[1] >= '0' && p[1] <= '7'; k++) v = v * 8 + (*++p - '0'); sb_addc (&b, (char) v); break; }
		default: sb_addc (&b, '\\'); sb_addc (&b, *p);
		}
	    }
	  else sb_addc (&b, *p);
	}
    }
done:
  if (nl) sb_addc (&b, '\n');
  out_write (&cur_out, b.s, b.n);
  sb_free (&b);
  return 0;
}

static int bi_printf (int argc, char **argv)
{
  const char *f;
  int ai = 2;
  str o; sb_init (&o); sb_adds (&o, "");
  if (argc < 2) { errs ("printf: usage: printf format [argument...]\n"); return 2; }
  for (;;)
    {
      int used = 0;
      for (f = argv[1]; *f; f++)
	{
	  if (*f == '\\')
	    {
	      f++;
	      switch (*f)
		{
		case 'n': sb_addc (&o, '\n'); break;
		case 't': sb_addc (&o, '\t'); break;
		case 'r': sb_addc (&o, '\r'); break;
		case 'a': sb_addc (&o, '\a'); break;
		case 'b': sb_addc (&o, '\b'); break;
		case 'f': sb_addc (&o, '\f'); break;
		case 'v': sb_addc (&o, '\v'); break;
		case '\\': sb_addc (&o, '\\'); break;
		case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': { int v = 0, k; for (k = 0; k < 3 && *f >= '0' && *f <= '7'; k++, f++) v = v * 8 + (*f - '0'); f--; sb_addc (&o, (char) v); break; }
		case 0: f--; break;
		default: sb_addc (&o, '\\'); sb_addc (&o, *f);
		}
	    }
	  else if (*f == '%')
	    {
	      char spec[64]; size_t sl = 0; char conv; char tmp[512];
	      spec[sl++] = *f++;
	      if (*f == '%') { sb_addc (&o, '%'); continue; }
	      while (*f && strchr ("-+ #0123456789.*", *f) && sl < sizeof spec - 8) spec[sl++] = *f++;
	      conv = *f;
	      if (!conv) break;
	      if (conv == 's' || conv == 'b')
		{
		  const char *a = ai < argc ? argv[ai++] : "";
		  used = 1;
		  spec[sl++] = 's'; spec[sl] = 0;
		  if (conv == 'b')					/* the argument with backslash escapes */
		    {
		      const char *q;
		      for (q = a; *q; q++)
			{
			  if (*q != '\\' || !q[1]) { sb_addc (&o, *q); continue; }
			  q++;
			  switch (*q)
			    {
			    case 'n': sb_addc (&o, '\n'); break; case 't': sb_addc (&o, '\t'); break; case 'r': sb_addc (&o, '\r'); break;
			    case 'a': sb_addc (&o, '\a'); break; case 'b': sb_addc (&o, '\b'); break; case 'f': sb_addc (&o, '\f'); break; case 'v': sb_addc (&o, '\v'); break;
			    case '\\': sb_addc (&o, '\\'); break;
			    case 'c': goto printf_done;
			    case '0': { int v = 0, k; for (k = 0; k < 3 && q[1] >= '0' && q[1] <= '7'; k++) v = v * 8 + (*++q - '0'); sb_addc (&o, (char) v); break; }
			    default: sb_addc (&o, '\\'); sb_addc (&o, *q);
			    }
			}
		      continue;
		    }
		  { str t; sb_init (&t); sb_adds (&t, ""); { int w = snprintf (tmp, sizeof tmp, spec, a); if (w >= (int) sizeof tmp) { char *big = malloc ((size_t) w + 1); snprintf (big, (size_t) w + 1, spec, a); sb_adds (&o, big); free (big); } else sb_adds (&o, tmp); } sb_free (&t); }
		}
	      else if (conv == 'c') { const char *a = ai < argc ? argv[ai++] : ""; used = 1; sb_addc (&o, *a); }
	      else if (strchr ("diouxX", conv))
		{
		  const char *a = ai < argc ? argv[ai++] : "0";
		  long v = a[0] == '\'' ? (unsigned char) a[1] : strtol (a, 0, 0);
		  used = 1;
		  spec[sl++] = 'l'; spec[sl++] = conv; spec[sl] = 0;
		  snprintf (tmp, sizeof tmp, spec, v); sb_adds (&o, tmp);
		}
	      else if (strchr ("eEfFgG", conv))
		{
		  const char *a = ai < argc ? argv[ai++] : "0";
		  used = 1;
		  spec[sl++] = conv; spec[sl] = 0;
		  snprintf (tmp, sizeof tmp, spec, strtod (a, 0)); sb_adds (&o, tmp);
		}
	      else { sb_addc (&o, '%'); sb_addc (&o, conv); }
	    }
	  else sb_addc (&o, *f);
	}
      if (!used || ai >= argc) break;
    }
printf_done:
  out_write (&cur_out, o.s, o.n);
  sb_free (&o);
  return 0;
}

static int bi_cd (int argc, char **argv)
{
  const char *dir = argc > 1 ? argv[1] : get_var ("HOME");
  char cwd[1024];
  if (argc > 1 && !strcmp (argv[1], "-")) { dir = get_var ("OLDPWD"); if (dir) { outs (dir); outs ("\n"); } }
  if (!dir) { errs ("cd: no home folder\n"); return 1; }
  if (getcwd (cwd, sizeof cwd)) set_var ("OLDPWD", cwd);
  if (chdir (dir) != 0) { errf ("sh: cd: %s: %s\n", dir, strerror (errno)); return 1; }
  if (getcwd (cwd, sizeof cwd)) set_var ("PWD", cwd);
  return 0;
}
static int bi_pwd (int argc, char **argv) { char cwd[1024]; (void) argc; (void) argv; if (!getcwd (cwd, sizeof cwd)) return 1; outs (cwd); outs ("\n"); return 0; }

static int test_expr (char **a, int n, int *pos);
static int file_test (char op, const char *f)
{
  struct stat st;
  switch (op)
    {
    case 'e': return stat (f, &st) == 0;
    case 'f': return stat (f, &st) == 0 && S_ISREG (st.st_mode);
    case 'd': return stat (f, &st) == 0 && S_ISDIR (st.st_mode);
    case 'r': case 'w': case 'x': return stat (f, &st) == 0;
    case 's': return stat (f, &st) == 0 && st.st_size > 0;
    case 'L': case 'h': return lstat (f, &st) == 0 && S_ISLNK (st.st_mode);
    case 't': return isatty (atoi (f));
    }
  return 0;
}
static int test_primary (char **a, int n, int *pos)
{
  int p = *pos;
  if (p >= n) return 0;
  if (!strcmp (a[p], "!")) { (*pos)++; return !test_primary (a, n, pos); }
  if (!strcmp (a[p], "(")) { int r; (*pos)++; r = test_expr (a, n, pos); if (*pos < n && !strcmp (a[*pos], ")")) (*pos)++; return r; }
  if (p + 2 < n || (p + 2 == n && 0))
    {
      const char *op = a[p + 1];
      if (!strcmp (op, "=") || !strcmp (op, "==") ) { *pos += 3; return !strcmp (a[p], a[p + 2]); }
      if (!strcmp (op, "!=")) { *pos += 3; return strcmp (a[p], a[p + 2]) != 0; }
      if (!strcmp (op, "-eq")) { *pos += 3; return atol (a[p]) == atol (a[p + 2]); }
      if (!strcmp (op, "-ne")) { *pos += 3; return atol (a[p]) != atol (a[p + 2]); }
      if (!strcmp (op, "-lt")) { *pos += 3; return atol (a[p]) < atol (a[p + 2]); }
      if (!strcmp (op, "-le")) { *pos += 3; return atol (a[p]) <= atol (a[p + 2]); }
      if (!strcmp (op, "-gt")) { *pos += 3; return atol (a[p]) > atol (a[p + 2]); }
      if (!strcmp (op, "-ge")) { *pos += 3; return atol (a[p]) >= atol (a[p + 2]); }
      if (!strcmp (op, "-nt") || !strcmp (op, "-ot"))
	{
	  struct stat s1, s2; int ok1 = stat (a[p], &s1) == 0, ok2 = stat (a[p + 2], &s2) == 0;
	  *pos += 3;
	  if (op[1] == 'n') return ok1 && (!ok2 || s1.st_mtime > s2.st_mtime);
	  return ok2 && (!ok1 || s1.st_mtime < s2.st_mtime);
	}
    }
  if (p + 1 < n && a[p][0] == '-' && a[p][1] && !a[p][2])
    {
      char op = a[p][1];
      if (op == 'z') { *pos += 2; return a[p + 1][0] == 0; }
      if (op == 'n') { *pos += 2; return a[p + 1][0] != 0; }
      if (strchr ("efdrwxsLht", op)) { *pos += 2; return file_test (op, a[p + 1]); }
    }
  (*pos)++;
  return a[p][0] != 0;
}
static int test_and (char **a, int n, int *pos)
{
  int r = test_primary (a, n, pos);
  while (*pos < n && !strcmp (a[*pos], "-a")) { (*pos)++; { int r2 = test_primary (a, n, pos); r = r && r2; } }
  return r;
}
static int test_expr (char **a, int n, int *pos)
{
  int r = test_and (a, n, pos);
  while (*pos < n && !strcmp (a[*pos], "-o")) { (*pos)++; { int r2 = test_and (a, n, pos); r = r || r2; } }
  return r;
}
static int bi_test (int argc, char **argv)
{
  int pos = 1, n = argc, r;
  if (!strcmp (argv[0], "[")) { if (argc < 2 || strcmp (argv[argc - 1], "]")) { errs ("sh: [: ] expected\n"); return 2; } n = argc - 1; }
  if (n <= 1) return 1;
  r = test_expr (argv, n, &pos);
  return r ? 0 : 1;
}

static int read_line_from_input (str *line, int raw)
{
  int c, got = 0;
  FILE *f = stdin;
  if (cur_in_path)
    {
      if (!cur_in_fp) { cur_in_fp = fopen (cur_in_path, "r"); if (cur_in_fp && cur_in_fd >= 3) fdtab[cur_in_fd].in_fp = cur_in_fp; }
      if (!cur_in_fp) return -1;
      f = cur_in_fp;
    }
  fflush (stdout);
  sb_adds (line, "");
  while ((c = fgetc (f)) != EOF)
    {
      got = 1;
      if (c == '\n') return 0;
      if (c == '\\' && !raw) { int d = fgetc (f); if (d == '\n') continue; if (d != EOF) { sb_addc (line, (char) d); continue; } }
      sb_addc (line, (char) c);
    }
  return got ? 0 : -1;
}
static int bi_read (int argc, char **argv)
{
  int i = 1, raw = 0, r;
  str line; sb_init (&line);
  const char *into_arr = 0;
  while (i < argc && argv[i][0] == '-' && argv[i][1]) { if (!strcmp (argv[i], "-r")) raw = 1; else if (!strcmp (argv[i], "-a") && i + 1 < argc) { into_arr = argv[++i]; } i++; }
  r = read_line_from_input (&line, raw);
  if (into_arr)
    {
      struct arr *a = make_arr (into_arr);
      const char *p = line.s ? line.s : "", *ifs = ifs_chars ();
      int k = 0;
      arr_clear (a);
      for (;;)
	{
	  size_t n;
	  while (*p && strchr (ifs, *p) && (*p == ' ' || *p == '\t')) p++;
	  if (!*p) break;
	  n = 0; while (p[n] && !strchr (ifs, p[n])) n++;
	  { char *w = strndup (p, n); arr_set (a, k++, w); free (w); }
	  p += n; if (*p) p++;
	}
      if (!k) arr_sync (a);
      sb_free (&line);
      return r < 0 ? 1 : 0;
    }
  if (i >= argc) { set_var ("REPLY", line.s ? line.s : ""); sb_free (&line); return r < 0 ? 1 : 0; }
  {
    const char *p = line.s ? line.s : "", *ifs = ifs_chars ();
    for (; i < argc; i++)
      {
	size_t n;
	while (*p && (*p == ' ' || *p == '\t') && strchr (ifs, *p)) p++;
	if (i == argc - 1)
	  {
	    size_t pl = strlen (p);
	    while (pl > 0 && (p[pl - 1] == ' ' || p[pl - 1] == '\t' || p[pl - 1] == '\n') && strchr (ifs, p[pl - 1])) pl--;
	    { char *w = strndup (p, pl); set_var (argv[i], w); free (w); }
	    break;
	  }
	n = 0; while (p[n] && !strchr (ifs, p[n])) n++;
	{ char *w = strndup (p, n); set_var (argv[i], w); free (w); }
	p += n; if (*p) p++;
      }
  }
  sb_free (&line);
  return r < 0 ? 1 : 0;
}

/* getopts optstring name [arg...]: the options of the positional parameters (or the arguments) one at a time; OPTIND is the next argument, OPTARG the argument of the option */
static int bi_getopts (int argc, char **argv)
{
  static int optpos;				/* the character inside a bundle (-abc) that is next, 0 at the start of an argument */
  static int last_optind;
  const char *spec, *name;
  char **args;
  int nargs, silent, optind_, opt_len;
  const char *ov = get_var ("OPTIND");
  char buf[32], optc[2];
  const char *cur, *p;
  if (argc < 3) { errs ("getopts: usage: getopts optstring name [arg ...]\n"); return 2; }
  spec = argv[1]; name = argv[2];
  silent = spec[0] == ':';
  if (silent) spec++;
  if (argc > 3) { args = argv + 3; nargs = argc - 3; } else { args = pos_params; nargs = npos; }
  optind_ = ov ? atoi (ov) : 1;
  if (optind_ < 1 || optind_ != last_optind) { optpos = 0; if (optind_ < 1) optind_ = 1; }	/* OPTIND was set by the script: start again */
  unset_var ("OPTARG");
  if (optind_ > nargs) goto done_opts;
  cur = args[optind_ - 1];
  if (optpos == 0)
    {
      if (cur[0] != '-' || cur[1] == 0) goto done_opts;		/* not an option: the end */
      if (!strcmp (cur, "--")) { optind_++; goto done_opts; }
      optpos = 1;
    }
  optc[0] = cur[optpos]; optc[1] = 0;
  opt_len = (int) strlen (cur);
  p = strchr (spec, cur[optpos]);
  optpos++;
  if (!p || cur[optpos - 1] == ':')
    {
      if (silent) set_var ("OPTARG", optc);
      else { errf ("Illegal option -%c\n", optc[0]); }
      set_var (name, "?");
      if (optpos >= opt_len) { optpos = 0; optind_++; }
      snprintf (buf, sizeof buf, "%d", optind_); set_var ("OPTIND", buf); last_optind = optind_;
      return 0;
    }
  if (p[1] == ':')					/* the option takes an argument: the rest of this one, or the next */
    {
      if (optpos < opt_len) { set_var ("OPTARG", cur + optpos); optind_++; }
      else if (optind_ < nargs) { set_var ("OPTARG", args[optind_]); optind_ += 2; }
      else
	{
	  optpos = 0;
	  if (silent) { set_var ("OPTARG", optc); set_var (name, ":"); }
	  else { errf ("Option requires an argument -- %c\n", optc[0]); set_var (name, "?"); }
	  optind_++;
	  snprintf (buf, sizeof buf, "%d", optind_); set_var ("OPTIND", buf); last_optind = optind_;
	  return 0;
	}
      optpos = 0;
    }
  else if (optpos >= opt_len) { optpos = 0; optind_++; }
  set_var (name, optc);
  snprintf (buf, sizeof buf, "%d", optind_); set_var ("OPTIND", buf); last_optind = optind_;
  return 0;
done_opts:
  optpos = 0;
  set_var (name, "?");
  snprintf (buf, sizeof buf, "%d", optind_); set_var ("OPTIND", buf); last_optind = optind_;
  return 1;
}

static int bi_export (int argc, char **argv)
{
  int i;
  for (i = 1; i < argc; i++)
    {
      char *eq = strchr (argv[i], '=');
      if (!strcmp (argv[i], "-p")) continue;
      if (eq) { *eq = 0; export_var (argv[i], eq + 1); *eq = '='; }
      else export_var (argv[i], 0);
    }
  if (argc == 1 || (argc == 2 && !strcmp (argv[1], "-p")))
    { struct var *v; for (v = vars; v; v = v->next) if (v->exported && v->value) out_printf (&cur_out, "export %s='%s'\n", v->name, v->value); }
  return 0;
}
static int bi_readonly (int argc, char **argv)
{
  int i;
  for (i = 1; i < argc; i++)
    {
      char *eq = strchr (argv[i], '=');
      struct var *v;
      if (eq) { *eq = 0; set_var (argv[i], eq + 1); }
      v = find_var (argv[i]);
      if (v) v->readonly = 1;
      if (eq) *eq = '=';
    }
  return 0;
}
static int bi_declare (int argc, char **argv)
{
  int i, isarr = 0, ro = 0, ex = 0, print = 0, r = 0;
  for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1]; i++)
    {
      const char *c;
      for (c = argv[i] + 1; *c; c++) { if (*c == 'a' || *c == 'A') isarr = 1; else if (*c == 'r') ro = 1; else if (*c == 'x') ex = 1; else if (*c == 'p') print = 1; }
    }
  for (; i < argc; i++)
    {
      char *eq = strchr (argv[i], '='), *nm = eq ? strndup (argv[i], (size_t) (eq - argv[i])) : strdup (argv[i]);
      if (print)
	{
	  struct arr *a = find_arr (nm);
	  const char *v = get_var (nm);
	  if (a) { int k, first = 1; out_printf (&cur_out, "declare -a %s=(", nm); for (k = 0; k < a->n; k++) if (a->v[k]) { out_printf (&cur_out, "%s[%d]=\"%s\"", first ? "" : " ", k, a->v[k]); first = 0; } out_printf (&cur_out, ")\n"); }
	  else if (v) out_printf (&cur_out, "declare -- %s=\"%s\"\n", nm, v);
	  else r = 1;
	  free (nm);
	  continue;
	}
      if (eq && do_assign (argv[i])) r = 1;
      else if (!eq && isarr) make_arr (nm);
      if (ro) { struct var *v = find_var (nm); if (v) v->readonly = 1; }
      if (ex) export_var (nm, 0);
      free (nm);
    }
  return r;
}
static int bi_unset (int argc, char **argv)
{
  int i;
  for (i = 1; i < argc; i++)
    {
      if (!strcmp (argv[i], "-f") || !strcmp (argv[i], "-v")) continue;
      {
	char *br = strchr (argv[i], '[');
	if (br && argv[i][strlen (argv[i]) - 1] == ']')
	  {
	    char *nm = strndup (argv[i], (size_t) (br - argv[i])), *ix = strndup (br + 1, strlen (br) - 2), *ex = expand_str (ix);
	    struct arr *a = find_arr (nm);
	    long k = arith (ex);
	    if (a && k >= 0 && k < a->n) { free (a->v[k]); a->v[k] = 0; arr_sync (a); }
	    free (nm); free (ix); free (ex);
	    continue;
	  }
      }
      unset_var (argv[i]);
    }
  return 0;
}
static void set_positional (int argc, char **argv)
{
  int i;
  pos_params = malloc ((size_t) (argc + 1) * sizeof *pos_params);
  for (i = 0; i < argc; i++) pos_params[i] = strdup (argv[i]);
  npos = argc;
}
static int bi_set (int argc, char **argv)
{
  int i = 1;
  if (argc == 1) { struct var *v; for (v = vars; v; v = v->next) if (v->value) out_printf (&cur_out, "%s='%s'\n", v->name, v->value); return 0; }
  for (; i < argc; i++)
    {
      const char *a = argv[i];
      if (!strcmp (a, "--")) { i++; break; }
      if ((a[0] == '-' || a[0] == '+') && a[1])
	{
	  int on = a[0] == '-';
	  const char *c;
	  for (c = a + 1; *c; c++)
	    {
	      if (*c == 'e') opt_e = on; else if (*c == 'x') opt_x = on; else if (*c == 'u') opt_u = on; else if (*c == 'f') opt_f = on; else if (*c == 'n') opt_n = on;
	      else if (*c == 'o')
		{
		  const char *name = i + 1 < argc ? argv[++i] : 0;
		  if (!name) { outs (opt_pipefail ? "pipefail on\n" : "pipefail off\n"); }
		  else if (!strcmp (name, "pipefail")) opt_pipefail = on;
		  else if (!strcmp (name, "errexit")) opt_e = on;
		  else if (!strcmp (name, "nounset")) opt_u = on;
		  else if (!strcmp (name, "noglob")) opt_f = on;
		  else if (!strcmp (name, "xtrace")) opt_x = on;
		  else if (!strcmp (name, "noexec")) opt_n = on;
		}
	    }
	}
      else break;
    }
  if (i < argc || (argc >= 2 && !strcmp (argv[argc - 1], "--"))) set_positional (argc - i, argv + i);
  return 0;
}
static int bi_shift (int argc, char **argv)
{
  int n = argc > 1 ? atoi (argv[1]) : 1, i;
  if (n > npos) return 1;
  for (i = 0; i + n < npos; i++) pos_params[i] = pos_params[i + n];
  npos -= n;
  return 0;
}
static int bi_exit (int argc, char **argv) { exit_code = argc > 1 ? atoi (argv[1]) & 255 : last_status; exit_flag = 1; return exit_code; }
static int bi_return (int argc, char **argv) { int r = argc > 1 ? atoi (argv[1]) & 255 : last_status; ret_flag = 1; return r; }
static int bi_break (int argc, char **argv) { brk_n = argc > 1 ? atoi (argv[1]) : 1; if (brk_n < 1) brk_n = 1; return 0; }
static int bi_continue (int argc, char **argv) { cont = argc > 1 ? atoi (argv[1]) : 1; if (cont < 1) cont = 1; return 0; }
static int bi_colon (int argc, char **argv) { (void) argc; (void) argv; return 0; }
static int bi_false (int argc, char **argv) { (void) argc; (void) argv; return 1; }
static int bi_eval (int argc, char **argv)
{
  str b; int i, r;
  sb_init (&b); sb_adds (&b, "");
  for (i = 1; i < argc; i++) { if (i > 1) sb_addc (&b, ' '); sb_adds (&b, argv[i]); }
  r = exec_string (b.s, "eval");
  sb_free (&b);
  return r;
}
static int run_file (const char *path, int pass_args, int argc, char **argv);
static int bi_dot (int argc, char **argv)
{
  char *p;
  int r;
  if (argc < 2) { errs ("sh: .: file name required\n"); return 2; }
  p = find_in_path (argv[1]);
  if (!p) p = strdup (argv[1]);
  r = run_file (p, argc > 2, argc - 2, argv + 2);
  free (p);
  return r;
}
static int bi_local (int argc, char **argv)
{
  int i;
  for (i = 1; i < argc; i++)
    {
      char *eq;
      struct localsave *l;
      const char *old;
      if (argv[i][0] == '-' && argv[i][1]) continue;
      eq = strchr (argv[i], '=');
      l = calloc (1, sizeof *l);
      if (eq) *eq = 0;
      old = get_var (argv[i]);
      l->name = strdup (argv[i]); l->had = old != 0; l->old = old ? strdup (old) : 0;
      l->next = locals; locals = l;
      if (eq) { *eq = '='; do_assign (argv[i]); } else unset_var (argv[i]);
    }
  return 0;
}
/* ---- traps: EXIT, ERR, and the signals that can arrive here (kill -SIG $$, Escape = INT, alarm ...); a handler only notes the signal, the trap command runs after the current command ---- */
static const struct { const char *name; int num; } sig_names[] = {
  { "HUP", SIGHUP }, { "INT", SIGINT }, { "QUIT", SIGQUIT }, { "ILL", SIGILL }, { "ABRT", SIGABRT }, { "FPE", SIGFPE }, { "KILL", SIGKILL }, { "SEGV", SIGSEGV }, { "PIPE", SIGPIPE },
  { "ALRM", SIGALRM }, { "TERM", SIGTERM }, { "USR1", SIGUSR1 }, { "USR2", SIGUSR2 }, { "CHLD", SIGCHLD }, { 0, 0 } };
#define NTRAP 64
static char *traps[NTRAP];
static volatile sig_atomic_t trap_pending[NTRAP], any_trap;
static char *trap_err;
static int in_trap;
static void on_trap_signal (int sig) { if (sig > 0 && sig < NTRAP) { trap_pending[sig] = 1; any_trap = 1; } }
static int sig_from_name (const char *n)
{
  int i;
  if (isdigit ((unsigned char) n[0])) return atoi (n);
  if (!strncmp (n, "SIG", 3)) n += 3;
  for (i = 0; sig_names[i].name; i++) if (!strcmp (n, sig_names[i].name)) return sig_names[i].num;
  return -1;
}
static int exec_string (const char *text, const char *name);
static void run_pending_traps (void)
{
  int sig;
  if (in_trap) return;
  any_trap = 0;
  for (sig = 1; sig < NTRAP; sig++)
    if (trap_pending[sig])
      {
	trap_pending[sig] = 0;
	if (traps[sig] && traps[sig][0])
	  {
	    int saved = last_status;
	    in_trap++;
	    exec_string (traps[sig], "trap");
	    in_trap--;
	    last_status = saved;
	  }
      }
}
static void run_err_trap (void)
{
  int saved = last_status;
  char *t;
  if (!trap_err || !trap_err[0] || in_trap) return;
  t = strdup (trap_err);
  in_trap++;
  exec_string (t, "trap");
  in_trap--;
  free (t);
  last_status = saved;
}
static int bi_trap (int argc, char **argv)
{
  int i = 1, sig;
  const char *action;
  if (argc == 1 || (argc == 2 && !strcmp (argv[1], "-p")))
    {
      if (trap_exit) out_printf (&cur_out, "trap -- '%s' EXIT\n", trap_exit);
      if (trap_err) out_printf (&cur_out, "trap -- '%s' ERR\n", trap_err);
      for (sig = 1; sig < NTRAP; sig++) if (traps[sig]) { int k; for (k = 0; sig_names[k].name; k++) if (sig_names[k].num == sig) out_printf (&cur_out, "trap -- '%s' SIG%s\n", traps[sig], sig_names[k].name); }
      return 0;
    }
  if (!strcmp (argv[1], "-l")) { int k; for (k = 0; sig_names[k].name; k++) out_printf (&cur_out, "%d) SIG%s\n", sig_names[k].num, sig_names[k].name); return 0; }
  if (!strcmp (argv[1], "--")) i++;
  if (i >= argc) return 0;
  if (isdigit ((unsigned char) argv[i][0]) && !argv[i][1 + strspn (argv[i] + 1, "0123456789")]) action = "-";		/* trap 2 : reset */
  else action = argv[i++];
  for (; i < argc; i++)
    {
      const char *nm = argv[i];
      int reset = !strcmp (action, "-"), ignore = !action[0];
      if (!strcmp (nm, "EXIT") || !strcmp (nm, "0")) { free (trap_exit); trap_exit = reset ? 0 : strdup (action); continue; }
      if (!strcmp (nm, "ERR")) { free (trap_err); trap_err = reset ? 0 : strdup (action); continue; }
      if (!strcmp (nm, "DEBUG") || !strcmp (nm, "RETURN")) continue;
      sig = sig_from_name (nm);
      if (sig <= 0 || sig >= NTRAP || sig == SIGKILL) { errf ("sh: trap: %s: bad trap\n", nm); return 1; }
      free (traps[sig]); traps[sig] = 0;
      if (reset) signal (sig, SIG_DFL);
      else if (ignore) { traps[sig] = strdup (""); signal (sig, SIG_IGN); }
      else { traps[sig] = strdup (action); signal (sig, on_trap_signal); }
    }
  return 0;
}
static int bi_kill (int argc, char **argv)
{
  int i = 1, sig = SIGTERM, r = 0;
  if (argc >= 2 && !strcmp (argv[1], "-l")) { int k; for (k = 0; sig_names[k].name; k++) out_printf (&cur_out, "%s%s", k ? " " : "", sig_names[k].name); outs ("\n"); return 0; }
  if (argc >= 3 && !strcmp (argv[1], "-s")) { sig = sig_from_name (argv[2]); i = 3; }
  else if (argc >= 2 && argv[1][0] == '-' && argv[1][1]) { sig = sig_from_name (argv[1] + 1); i = 2; }
  if (sig < 0) { errf ("sh: kill: bad signal\n"); return 1; }
  if (i >= argc) { errf ("sh: kill: usage: kill [-s sig] pid ...\n"); return 2; }
  for (; i < argc; i++)
    {
      int pid = atoi (argv[i]);
      if (pid == (int) getpid () || pid == 0) { if (sig == 0) continue; raise (sig); }
      else if (pid > 0 && job_known (pid)) { /* a background job has already run to its end */ errf ("sh: kill: (%d) - No such process\n", pid); r = 1; }
      else { errf ("sh: kill: (%d) - No such process\n", pid); r = 1; }
    }
  return r;
}
/* ---- aliases: replaced in the text before the parser reads a command word (see parse_simple) ---- */
struct alias_ { char *name, *value; struct alias_ *next; };
static struct alias_ *aliases_;
static const char *find_alias (const char *name) { struct alias_ *a; for (a = aliases_; a; a = a->next) if (!strcmp (a->name, name)) return a->value; return 0; }
static int bi_alias (int argc, char **argv)
{
  int i, r = 0;
  if (argc == 1) { struct alias_ *a; for (a = aliases_; a; a = a->next) out_printf (&cur_out, "alias %s='%s'\n", a->name, a->value); return 0; }
  for (i = 1; i < argc; i++)
    {
      char *eq = strchr (argv[i], '=');
      if (!eq) { const char *v = find_alias (argv[i]); if (v) out_printf (&cur_out, "alias %s='%s'\n", argv[i], v); else { errf ("sh: alias: %s: not found\n", argv[i]); r = 1; } continue; }
      {
	char *nm = strndup (argv[i], (size_t) (eq - argv[i]));
	struct alias_ *a;
	for (a = aliases_; a; a = a->next) if (!strcmp (a->name, nm)) break;
	if (a) { free (a->value); a->value = strdup (eq + 1); free (nm); }
	else { a = calloc (1, sizeof *a); a->name = nm; a->value = strdup (eq + 1); a->next = aliases_; aliases_ = a; }
      }
    }
  return r;
}
static int bi_unalias (int argc, char **argv)
{
  int i, r = 0;
  for (i = 1; i < argc; i++)
    {
      struct alias_ **pp;
      if (!strcmp (argv[i], "-a")) { while (aliases_) { struct alias_ *a = aliases_; aliases_ = a->next; free (a->name); free (a->value); free (a); } continue; }
      for (pp = &aliases_; *pp; pp = &(*pp)->next)
	if (!strcmp ((*pp)->name, argv[i])) { struct alias_ *a = *pp; *pp = a->next; free (a->name); free (a->value); free (a); break; }
      if (!*pp && !find_alias (argv[i])) { /* removed above, or not there */ }
    }
  return r;
}
static int bi_wait (int argc, char **argv)
{
  int i, r = 0;
  for (i = 1; i < argc; i++) { int pid = atoi (argv[i]); r = job_known (pid) ? job_status (pid) : 127; }
  return r;
}
static int bi_jobs (int argc, char **argv) { (void) argc; (void) argv; return 0; }
static int bi_umask (int argc, char **argv) { (void) argc; (void) argv; if (argc == 1) outs ("0022\n"); return 0; }
static int bi_true0 (int argc, char **argv) { (void) argc; (void) argv; return 0; }
static int bi_sleep (int argc, char **argv)
{
  double s = argc > 1 ? atof (argv[1]) : 0;
  struct timespec ts;
  ts.tv_sec = (time_t) s; ts.tv_nsec = (long) ((s - (double) ts.tv_sec) * 1e9);
  fflush (stdout);
  nanosleep (&ts, 0);
  return 0;
}
static int bi_basename (int argc, char **argv)
{
  char *p, *e; size_t n;
  if (argc < 2) return 1;
  p = strdup (argv[1]);
  n = strlen (p);
  while (n > 1 && p[n - 1] == '/') p[--n] = 0;
  e = strrchr (p, '/');
  e = (e && n > 1) ? e + 1 : p;
  if (argc > 2) { size_t sl = strlen (argv[2]), el = strlen (e); if (sl < el && !strcmp (e + el - sl, argv[2])) e[el - sl] = 0; }
  outs (e); outs ("\n");
  free (p);
  return 0;
}
static int bi_dirname (int argc, char **argv)
{
  char *p, *e; size_t n;
  if (argc < 2) return 1;
  p = strdup (argv[1]);
  n = strlen (p);
  while (n > 1 && p[n - 1] == '/') p[--n] = 0;
  e = strrchr (p, '/');
  if (!e) outs (".");
  else if (e == p) outs ("/");
  else { *e = 0; outs (p); }
  outs ("\n");
  free (p);
  return 0;
}
static int bi_expr (int argc, char **argv)
{
  /* expr: the arithmetic comparisons and the string operations that scripts use: A OP B, "length S", "substr S P N", "index S C", S : REGEX (basic prefix match with *, no groups) */
  char num[32];
  if (argc == 2) { outs (argv[1]); outs ("\n"); return !strcmp (argv[1], "0") || !argv[1][0]; }
  if (argc == 3 && !strcmp (argv[1], "length")) { snprintf (num, sizeof num, "%zu", strlen (argv[2])); outs (num); outs ("\n"); return 0; }
  if (argc == 4)
    {
      const char *op = argv[2]; long a = atol (argv[1]), b = atol (argv[3]); long r;
      if (!strcmp (op, "+")) r = a + b; else if (!strcmp (op, "-")) r = a - b; else if (!strcmp (op, "*")) r = a * b;
      else if (!strcmp (op, "/")) { if (!b) return 2; r = a / b; } else if (!strcmp (op, "%")) { if (!b) return 2; r = a % b; }
      else if (!strcmp (op, "=")) r = !strcmp (argv[1], argv[3]); else if (!strcmp (op, "!=")) r = strcmp (argv[1], argv[3]) != 0;
      else if (!strcmp (op, "<")) r = a < b; else if (!strcmp (op, ">")) r = a > b; else if (!strcmp (op, "<=")) r = a <= b; else if (!strcmp (op, ">=")) r = a >= b;
      else if (!strcmp (op, ":"))
	{
	  /* the length of the match of a pattern (made from the regular expression by taking .* as *) at the start */
	  str pat; const char *q; size_t i, best = 0; int m = 0;
	  sb_init (&pat); sb_adds (&pat, "");
	  for (q = argv[3]; *q; q++) { if (q[0] == '.' && q[1] == '*') { sb_addc (&pat, '*'); q++; } else if (q[0] == '.') sb_addc (&pat, '?'); else if (strchr ("*?[", *q)) { sb_addc (&pat, '\\'); sb_addc (&pat, *q); } else sb_addc (&pat, *q); }
	  for (i = 0; i <= strlen (argv[1]); i++) { char *t = strndup (argv[1], i); if (fnmatch (pat.s, t, 0) == 0) { best = i; m = 1; } free (t); }
	  sb_free (&pat);
	  snprintf (num, sizeof num, "%zu", m ? best : 0); outs (num); outs ("\n");
	  return m ? 0 : 1;
	}
      else return 2;
      snprintf (num, sizeof num, "%ld", r); outs (num); outs ("\n");
      return r == 0;
    }
  if (argc == 5 && !strcmp (argv[1], "substr"))
    {
      long p = atol (argv[3]), n = atol (argv[4]); size_t l = strlen (argv[2]);
      if (p < 1 || (size_t) p > l || n < 1) { outs ("\n"); return 1; }
      out_write (&cur_out, argv[2] + p - 1, (size_t) n < l - (size_t) p + 1 ? (size_t) n : l - (size_t) p + 1);
      outs ("\n");
      return 0;
    }
  return 2;
}

/* ---- the small utilities that RISC OS lacks ---- */
static int copy_file (const char *from, const char *to)
{
  FILE *a = fopen (from, "rb"), *b;
  char buf[8192]; size_t n;
  if (!a) { errf ("cp: %s: %s\n", from, strerror (errno)); return 1; }
  b = fopen (to, "wb");
  if (!b) { errf ("cp: %s: %s\n", to, strerror (errno)); fclose (a); return 1; }
  while ((n = fread (buf, 1, sizeof buf, a)) > 0) fwrite (buf, 1, n, b);
  fclose (a); fclose (b);
  return 0;
}
static int is_dir (const char *p) { struct stat st; return stat (p, &st) == 0 && S_ISDIR (st.st_mode); }
static char *join_path (const char *dir, const char *file)
{
  const char *b = strrchr (file, '/');
  char *r = malloc (strlen (dir) + strlen (file) + 2);
  sprintf (r, "%s/%s", dir, b ? b + 1 : file);
  return r;
}
static int remove_tree (const char *p, int force)
{
  DIR *d = opendir (p);
  struct dirent *e;
  int r = 0;
  if (!d) return remove (p) == 0 || force ? 0 : 1;
  while ((e = readdir (d)) != 0)
    {
      char *c;
      if (!strcmp (e->d_name, ".") || !strcmp (e->d_name, "..")) continue;
      c = join_path (p, e->d_name);
      if (remove_tree (c, force)) r = 1;
      free (c);
    }
  closedir (d);
  if (rmdir (p) != 0 && !force) r = 1;
  return r;
}
static int bi_rm (int argc, char **argv)
{
  int i = 1, force = 0, rec = 0, r = 0;
  for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) { const char *c; for (c = argv[i] + 1; *c; c++) { if (*c == 'f') force = 1; if (*c == 'r' || *c == 'R') rec = 1; } }
  for (; i < argc; i++)
    {
      if (is_dir (argv[i])) { if (!rec) { errf ("rm: %s: is a folder\n", argv[i]); r = 1; } else if (remove_tree (argv[i], force)) r = 1; }
      else if (remove (argv[i]) != 0 && !force) { errf ("rm: %s: %s\n", argv[i], strerror (errno)); r = 1; }
    }
  return r;
}
static int mkdirs (const char *p)
{
  char *c = strdup (p), *q;
  int r = 0;
  for (q = c + 1; *q; q++) if (*q == '/') { *q = 0; if (!is_dir (c)) mkdir (c, 0777); *q = '/'; }
  if (!is_dir (c) && mkdir (c, 0777) != 0) r = 1;
  free (c);
  return r;
}
static int bi_mkdir (int argc, char **argv)
{
  int i = 1, parents = 0, r = 0;
  for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) if (strchr (argv[i], 'p')) parents = 1;
  for (; i < argc; i++)
    {
      if (parents) { if (mkdirs (argv[i])) { errf ("mkdir: %s: %s\n", argv[i], strerror (errno)); r = 1; } }
      else if (mkdir (argv[i], 0777) != 0) { errf ("mkdir: %s: %s\n", argv[i], strerror (errno)); r = 1; }
    }
  return r;
}
static int bi_rmdir (int argc, char **argv)
{
  int i, r = 0;
  for (i = 1; i < argc; i++) if (argv[i][0] != '-' && rmdir (argv[i]) != 0) { errf ("rmdir: %s: %s\n", argv[i], strerror (errno)); r = 1; }
  return r;
}
static int copy_tree (const char *from, const char *to)
{
  if (!is_dir (from)) return copy_file (from, to);
  {
    DIR *d = opendir (from); struct dirent *e; int r = 0;
    if (!d) return 1;
    if (!is_dir (to)) mkdir (to, 0777);
    while ((e = readdir (d)) != 0)
      {
	char *a, *b;
	if (!strcmp (e->d_name, ".") || !strcmp (e->d_name, "..")) continue;
	a = join_path (from, e->d_name); b = join_path (to, e->d_name);
	if (copy_tree (a, b)) r = 1;
	free (a); free (b);
      }
    closedir (d);
    return r;
  }
}
static int bi_cp (int argc, char **argv)
{
  int i = 1, rec = 0, r = 0, n;
  for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) if (strchr (argv[i], 'r') || strchr (argv[i], 'R')) rec = 1;
  n = argc - i;
  if (n < 2) { errs ("cp: usage: cp [-r] source... target\n"); return 1; }
  if (n > 2 || is_dir (argv[argc - 1]))
    {
      int k;
      for (k = i; k < argc - 1; k++) { char *t = join_path (argv[argc - 1], argv[k]); if (rec ? copy_tree (argv[k], t) : copy_file (argv[k], t)) r = 1; free (t); }
      return r;
    }
  return rec ? copy_tree (argv[i], argv[i + 1]) : copy_file (argv[i], argv[i + 1]);
}
static int bi_mv (int argc, char **argv)
{
  int i = 1, r = 0, n;
  for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) ;
  n = argc - i;
  if (n < 2) { errs ("mv: usage: mv source... target\n"); return 1; }
  {
    int k;
    for (k = i; k < argc - 1; k++)
      {
	char *t = (n > 2 || is_dir (argv[argc - 1])) ? join_path (argv[argc - 1], argv[k]) : strdup (argv[argc - 1]);
	if (rename (argv[k], t) != 0) { if (copy_tree (argv[k], t) == 0) remove_tree (argv[k], 1); else { errf ("mv: %s: %s\n", argv[k], strerror (errno)); r = 1; } }
	free (t);
      }
  }
  return r;
}
static int bi_cat (int argc, char **argv)
{
  int i, r = 0;
  char buf[4096]; size_t n;
  if (argc == 1 || (argc == 2 && !strcmp (argv[1], "-")))
    {
      FILE *f = stdin;
      if (cur_in_path) { if (!cur_in_fp) cur_in_fp = fopen (cur_in_path, "r"); f = cur_in_fp; }
      if (f) while ((n = fread (buf, 1, sizeof buf, f)) > 0) out_write (&cur_out, buf, n);
      return 0;
    }
  for (i = 1; i < argc; i++)
    {
      FILE *f;
      if (argv[i][0] == '-' && argv[i][1]) continue;
      f = fopen (argv[i], "rb");
      if (!f) { errf ("cat: %s: %s\n", argv[i], strerror (errno)); r = 1; continue; }
      while ((n = fread (buf, 1, sizeof buf, f)) > 0) out_write (&cur_out, buf, n);
      fclose (f);
    }
  return r;
}
static int bi_touch (int argc, char **argv)
{
  int i, r = 0;
  for (i = 1; i < argc; i++)
    {
      FILE *f;
      if (argv[i][0] == '-') continue;
      f = fopen (argv[i], "ab");
      if (!f) { errf ("touch: %s: %s\n", argv[i], strerror (errno)); r = 1; continue; }
      fclose (f);
      utimes (argv[i], 0);
    }
  return r;
}
static int bi_type (int argc, char **argv)
{
  int i, r = 0;
  for (i = 1; i < argc; i++)
    {
      char *p;
      if (!strcmp (argv[i], "-v") || !strcmp (argv[i], "-V")) continue;
      if (find_func (argv[i])) { out_printf (&cur_out, "%s is a function\n", argv[i]); continue; }
      p = find_in_path (argv[i]);
      if (p) { out_printf (&cur_out, "%s is %s\n", argv[i], p); free (p); }
      else { out_printf (&cur_out, "%s: not found\n", argv[i]); r = 1; }
    }
  return r;
}
static int bi_exec_dummy (int argc, char **argv) { (void) argc; (void) argv; return 0; }

struct builtin { const char *name; builtin_fn fn; int special; };
static const struct builtin builtins[] = {
  { ":", bi_colon, 1 }, { ".", bi_dot, 1 }, { "source", bi_dot, 1 }, { "break", bi_break, 1 }, { "continue", bi_continue, 1 }, { "eval", bi_eval, 1 }, { "exit", bi_exit, 1 }, { "export", bi_export, 1 },
  { "readonly", bi_readonly, 1 }, { "return", bi_return, 1 }, { "set", bi_set, 1 }, { "shift", bi_shift, 1 }, { "trap", bi_trap, 1 }, { "unset", bi_unset, 1 }, { "exec", bi_exec_dummy, 1 },
  { "echo", bi_echo, 0 }, { "printf", bi_printf, 0 }, { "cd", bi_cd, 0 }, { "pwd", bi_pwd, 0 }, { "declare", bi_declare, 0 }, { "typeset", bi_declare, 0 }, { "test", bi_test, 0 }, { "[", bi_test, 0 }, { "read", bi_read, 0 }, { "getopts", bi_getopts, 0 }, { "true", bi_true0, 0 }, { "false", bi_false, 0 },
  { "local", bi_local, 0 }, { "wait", bi_wait, 0 }, { "umask", bi_umask, 0 }, { "ulimit", bi_true0, 0 }, { "sleep", bi_sleep, 2 }, { "basename", bi_basename, 2 }, { "dirname", bi_dirname, 2 }, { "expr", bi_expr, 2 },
  { "rm", bi_rm, 2 }, { "mkdir", bi_mkdir, 2 }, { "rmdir", bi_rmdir, 2 }, { "cp", bi_cp, 2 }, { "mv", bi_mv, 2 }, { "cat", bi_cat, 2 }, { "touch", bi_touch, 2 }, { "chmod", bi_true0, 2 },
  { "type", bi_type, 0 }, { "command", 0, 0 }, { "hash", bi_true0, 0 }, { "alias", bi_alias, 0 }, { "unalias", bi_unalias, 0 }, { "kill", bi_kill, 0 }, { "jobs", bi_jobs, 0 }, { 0, 0, 0 } };
static const struct builtin *find_builtin (const char *name)
{
  const struct builtin *b;
  for (b = builtins; b->name; b++) if (!strcmp (b->name, name)) return b;
  return 0;
}

/* ------------------------------------------------------------------------------------------------------ executing */
static int call_function (struct func *f, int argc, char **argv)
{
  char **save_pos = pos_params; int save_npos = npos;
  struct localsave *save_locals = locals;
  int r;
  set_positional (argc - 1, argv + 1);
  func_depth++;
  r = exec_node (f->body);
  func_depth--;
  if (ret_flag) ret_flag = 0;
  while (locals != save_locals)
    {
      struct localsave *l = locals;
      if (l->had) set_var (l->name, l->old); else unset_var (l->name);
      locals = l->next;
      free (l->name); free (l->old); free (l);
    }
  pos_params = save_pos; npos = save_npos;
  return r;
}

/* apply redirections; the old state goes in *saved. Returns 0, or 1 when a file cannot be used. */
/* The descriptors 3 and up (exec 5>file, 3<&0, >&5 ...): each is an output (a sink) or an input (a file name, read through a FILE that stays open between commands). 0, 1 and 2 are cur_in_path, cur_out and cur_err. */
struct saved_io { struct sink out, err; char *in_path; FILE *in_fp; int err_null; list tmp; struct fdent fds[NFD]; int in_fd; };
static void save_io (struct saved_io *s)
{
  s->out = cur_out; s->err = cur_err; s->in_path = cur_in_path; s->in_fp = cur_in_fp; s->err_null = err_null; s->tmp.v = 0; s->tmp.n = s->tmp.cap = 0;
  memcpy (s->fds, fdtab, sizeof fdtab); s->in_fd = cur_in_fd;
  cur_out.fp = 0; cur_err.fp = 0;
}
static int fp_in_table (FILE *f)
{
  int i;
  for (i = 3; i < NFD; i++) if (fdtab[i].kind == 2 && fdtab[i].in_fp == f) return 1;
  return 0;
}
static int fp_in_saved (struct saved_io *s, FILE *f)
{
  int i;
  for (i = 3; i < NFD; i++) if (s->fds[i].kind == 2 && s->fds[i].in_fp == f) return 1;
  return 0;
}
static void restore_io (struct saved_io *s)
{
  int i;
  sink_close (&cur_out); sink_close (&cur_err);
  if (cur_in_path != s->in_path && cur_in_fp && !fp_in_table (cur_in_fp) && !fp_in_saved (s, cur_in_fp)) fclose (cur_in_fp);
  if (cur_in_path != s->in_path) cur_in_fp = s->in_fp;	/* a redirection of this scope ends; the same input goes on where it was */
  {
    struct fdent now[NFD];
    memcpy (now, fdtab, sizeof fdtab);
    memcpy (fdtab, s->fds, sizeof fdtab); cur_in_fd = s->in_fd;
    for (i = 3; i < NFD; i++)
      {
	if (now[i].kind != 2 || !now[i].in_fp || fdtab[i].in_fp == now[i].in_fp) continue;
	if (fdtab[i].kind == 2 && !fdtab[i].in_fp && fdtab[i].in_path && now[i].in_path && !strcmp (fdtab[i].in_path, now[i].in_path)) fdtab[i].in_fp = now[i].in_fp;	/* opened by a read in this scope: it goes on where it was */
	else if (!fp_in_saved (s, now[i].in_fp)) fclose (now[i].in_fp);						/* a descriptor of this scope ends */
      }
  }
  cur_out = s->out; cur_err = s->err; cur_in_path = s->in_path; err_null = s->err_null;
  for (i = 0; i < s->tmp.n; i++) { remove (s->tmp.v[i]); free (s->tmp.v[i]); }
  free (s->tmp.v);
}

/* the sink / input that descriptor n stands for now; 0 if it is not open */
static int fd_source (int n, struct fdent *e)
{
  memset (e, 0, sizeof *e);
  if (n == 0) { e->kind = 2; e->in_path = cur_in_path; e->in_fp = cur_in_fp; return cur_in_fd >= 3 ? (*e = fdtab[cur_in_fd], 1) : 1; }
  if (n == 1) { e->kind = 1; e->out = cur_out; e->out.fp = 0; return 1; }
  if (n == 2) { e->kind = 1; e->out = cur_err; e->out.fp = 0; return 1; }
  if (n >= 3 && n < NFD && fdtab[n].kind) { *e = fdtab[n]; e->out.fp = 0; return 1; }
  return 0;
}
/* make descriptor n stand for e */
static void fd_install (int n, const struct fdent *e)
{
  if (n == 0)
    {
      if (e->kind == 2) { cur_in_path = e->in_path; cur_in_fp = e->in_fp; cur_in_fd = -1; }
      else { cur_in_path = 0; cur_in_fp = 0; cur_in_fd = -1; }
    }
  else if (n == 1) { sink_close (&cur_out); if (e->kind == 1) { cur_out = e->out; cur_out.fp = 0; } else cur_out.kind = K_CONSOLE; }
  else if (n == 2) { sink_close (&cur_err); err_null = 0; if (e->kind == 1) { cur_err = e->out; cur_err.fp = 0; } else cur_err.kind = K_CONSOLE; }
  else if (n >= 3 && n < NFD) fdtab[n] = *e;
}
static void fd_close (int n)
{
  struct fdent e;
  memset (&e, 0, sizeof e);
  if (n == 0) { cur_in_path = 0; cur_in_fp = 0; }
  else if (n == 1 || n == 2) fd_install (n, &e);
  else if (n >= 3 && n < NFD)
    {
      FILE *f = fdtab[n].kind == 2 ? fdtab[n].in_fp : 0;
      int k, shared = f && f == cur_in_fp;
      for (k = 3; k < NFD && f && !shared; k++) if (k != n && fdtab[k].kind == 2 && fdtab[k].in_fp == f) shared = 1;
      if (f && !shared) fclose (f);					/* the last descriptor for the file: it is closed (so the file can be written, renamed, removed) */
      fdtab[n] = e;
    }
}
static void set_output_file (int n, char *path, int append)
{
  struct fdent e;
  memset (&e, 0, sizeof e);
  e.kind = 1; e.out.kind = K_PATH; e.out.path = path; e.out.append = append;
  fd_install (n, &e);
}
static void set_input_file (int n, char *path)
{
  struct fdent e;
  memset (&e, 0, sizeof e);
  e.kind = 2; e.in_path = path;
  fd_install (n, &e);
  if (n == 0) in_gen++;					/* a new input redirection: its file position starts at 0 */
  if (n >= 3) cur_in_fd = -1;
}

static int apply_redirs (struct redir *r, struct saved_io *s)
{
  for (; r; r = r->next)
    {
      char *t;
      int fd = r->fd;
      if (fd >= NFD) { errf ("sh: file descriptor %d is out of range\n", fd); return 1; }
      if (r->type == R_HERE)
	{
	  char *name = scrap_name (), *body;
	  FILE *f;
	  struct xctx x; list l = { 0, 0, 0 };
	  if (r->quoted) body = strdup (r->body ? r->body : "");
	  else
	    {
	      /* the body is expanded like text inside double quotes */
	      x.out = &l; x.split = 0; f_init (&x.cur);
	      expand_into (&x, r->body ? r->body : "", 1);
	      sb_free (&x.cur.mask);
	      body = sb_take (&x.cur.s);
	    }
	  f = fopen (name, "w");
	  if (!f) { errf ("sh: cannot make a here document\n"); free (body); free (name); return 1; }
	  fputs (body, f); fclose (f); free (body);
	  l_add (&s->tmp, name);
	  set_input_file (fd, name);
	  continue;
	}
      t = expand_str (r->target);
      if (r->type == R_IN || r->type == R_RW)
	{
	  if (!strcmp (t, "/dev/null")) { char *e = scrap_name (); FILE *f = fopen (e, "w"); if (f) fclose (f); l_add (&s->tmp, e); free (t); t = strdup (e); }
	  else if (!is_exec_file (t) && access (t, R_OK) != 0) { errf ("sh: cannot open %s: %s\n", t, strerror (errno)); free (t); return 1; }
	  set_input_file (fd, t);
	  continue;
	}
      if (r->type == R_OUT || r->type == R_APPEND)
	{
	  if (fd == 2 && !strcmp (t, "/dev/null")) { fd_close (2); err_null = 1; free (t); continue; }
	  if (r->type == R_OUT && strcmp (t, "/dev/null")) { FILE *f = fopen (t, "w"); if (!f) { errf ("sh: cannot create %s: %s\n", t, strerror (errno)); free (t); return 1; } fclose (f); }
	  else if (r->type == R_APPEND && strcmp (t, "/dev/null")) { FILE *f = fopen (t, "a"); if (f) fclose (f); }
	  set_output_file (fd, t, r->type == R_APPEND);
	  continue;
	}
      if (r->type == R_DUPOUT || r->type == R_DUPIN)
	{
	  if (!strcmp (t, "-")) fd_close (fd);
	  else if (isdigit ((unsigned char) t[0]))
	    {
	      struct fdent e;
	      int from = atoi (t);
	      if (from >= NFD || !fd_source (from, &e)) { errf ("sh: %d: bad file descriptor\n", from); free (t); return 1; }
	      fd_install (fd, &e);
	      if (fd == 0 && from >= 3) cur_in_fd = from;
	      if (fd >= 3 && e.kind == 2) fdtab[fd].in_fp = fdtab[from < NFD && from >= 3 ? from : fd].in_fp;
	    }
	  free (t);
	  continue;
	}
      free (t);
    }
  return 0;
}

static int status_of_cmd (int st) { return st; }

static void trace (list *words)
{
  int i;
  if (!opt_x) return;
  fputs ("+", stderr);
  for (i = 0; i < words->n; i++) { fputc (' ', stderr); fputs (words->v[i], stderr); }
  fputc ('\n', stderr);
}

/* ---- assignments: name=v  name+=v  name[i]=v  name=(a b c)  name+=(d)  name[i]+=v ---- */
static void arr_literal (struct arr *a, const char *inner, int append)
{
  /* the words of the text between ( and ): [i]=v items, and words that are expanded (split, globbed) */
  const char *p = inner;
  int next_idx = 0, i;
  if (!append) arr_clear (a);
  else for (i = 0; i < a->n; i++) if (a->v[i]) next_idx = i + 1;
  for (;;)
    {
      str w; int c;
      while (*p == ' ' || *p == '\t' || *p == '\n') p++;
      if (!*p) break;
      sb_init (&w); sb_adds (&w, "");
      while (*p && *p != ' ' && *p != '\t' && *p != '\n')
	{
	  c = *p++;
	  sb_addc (&w, (char) c);
	  if (c == '\\' && *p) sb_addc (&w, *p++);
	  else if (c == '\'') { while (*p && *p != '\'') sb_addc (&w, *p++); if (*p) sb_addc (&w, *p++); }
	  else if (c == '"') { while (*p && *p != '"') { if (*p == '\\' && p[1]) sb_addc (&w, *p++); sb_addc (&w, *p++); } if (*p) sb_addc (&w, *p++); }
	  else if (c == '$' && (*p == '(' || *p == '{'))
	    {
	      char open = *p, close = open == '(' ? ')' : '}';
	      int depth = 0;
	      while (*p) { sb_addc (&w, *p); if (*p == open) depth++; else if (*p == close && --depth == 0) { p++; break; } p++; }
	    }
	  else if (c == '`') { while (*p && *p != '`') { if (*p == '\\' && p[1]) sb_addc (&w, *p++); sb_addc (&w, *p++); } if (*p) sb_addc (&w, *p++); }
	}
      if (w.s[0] == '[' && strchr (w.s, ']') && strchr (w.s, ']')[1] == '=')
	{
	  char *close = strchr (w.s, ']'), *ix = strndup (w.s + 1, (size_t) (close - w.s - 1)), *ex = expand_str (ix), *val = expand_str (close + 2);
	  next_idx = (int) arith (ex);
	  arr_set (a, next_idx, val); next_idx++;
	  free (ix); free (ex); free (val);
	}
      else
	{
	  list l = { 0, 0, 0 };
	  expand_word (w.s, &l);
	  for (i = 0; i < l.n; i++) { arr_set (a, next_idx++, l.v[i]); free (l.v[i]); }
	  free (l.v);
	}
      sb_free (&w);
    }
  arr_sync (a);
}
static int do_assign (const char *raw)
{
  const char *e = raw;
  char name[256], *idx = 0;
  int plus = 0, rc = 0;
  size_t n;
  while (isalnum ((unsigned char) *e) || *e == '_') e++;
  n = (size_t) (e - raw);
  if (n >= sizeof name) return 1;
  memcpy (name, raw, n); name[n] = 0;
  if (*e == '[')
    {
      const char *q = e; int d = 0;
      for (; *q; q++) { if (*q == '[') d++; else if (*q == ']' && --d == 0) break; }
      idx = strndup (e + 1, (size_t) (q - e - 1));
      e = *q ? q + 1 : q;
    }
  if (*e == '+') { plus = 1; e++; }
  if (*e != '=') { free (idx); return 1; }
  e++;
  if (idx)
    {
      char *ex = expand_str (idx), *val = expand_str (e);
      struct arr *a = make_arr (name);
      long i = arith (ex);
      if (plus && i >= 0 && i < a->n && a->v[i]) { char *t = malloc (strlen (a->v[i]) + strlen (val) + 1); strcpy (t, a->v[i]); strcat (t, val); arr_set (a, (int) i, t); free (t); }
      else arr_set (a, (int) i, val);
      free (ex); free (val); free (idx);
      return 0;
    }
  if (*e == '(' && e[strlen (e) - 1] == ')')
    {
      char *inner = strndup (e + 1, strlen (e) - 2);
      struct arr *a = plus ? make_arr (name) : find_arr (name);
      if (!a) { a = make_arr (name); arr_clear (a); }
      arr_literal (a, inner, plus);
      free (inner);
      return 0;
    }
  {
    char *val = expand_str (e);
    if (plus)
      {
        const char *old = get_var (name);
        char *t = malloc ((old ? strlen (old) : 0) + strlen (val) + 1);
        strcpy (t, old ? old : ""); strcat (t, val);
        rc = set_var (name, t);
        free (t);
      }
    else rc = set_var (name, val);
    free (val);
  }
  return rc;
}

static int exec_simple_ (struct node *n);
static int exec_simple (struct node *n)
{
  clock_t t0 = profile ? clock () : 0;
  int r = exec_simple_ (n);
  if (profile)
    {
      long cs = (long) ((clock () - t0) * 100 / CLOCKS_PER_SEC);
      if (cs >= 20) errf ("sh: profile: %s took %ld cs\n", n->words.n ? n->words.v[0] : "(assignment)", cs);
    }
  return r;
}
static int exec_simple_ (struct node *n)
{
  list argv = { 0, 0, 0 };
  int i, r = 0;
  struct saved_io saved;
  if (n->line) { char ln[16]; snprintf (ln, sizeof ln, "%d", n->line); set_var ("LINENO", ln); }
  const struct builtin *b = 0;
  struct func *fn = 0;
  int keep_io = 0;
  for (i = 0; i < n->words.n; i++) expand_word (n->words.v[i], &argv);
  save_io (&saved);
  if (apply_redirs (n->redirs, &saved)) { restore_io (&saved); return 1; }
  if (argv.n == 0)
    {
      /* only assignments (and redirections: the files are made) */
      for (i = 0; i < n->assigns.n; i++)
	{
	  if (do_assign (n->assigns.v[i])) r = 1;
	}
      sink_open (&cur_out); sink_open (&cur_err);
      restore_io (&saved);
      return r;
    }
  trace (&argv);
  /* assignments before a command: for the command only */
  {
    struct { char *name; char *old; int had, exported; } *save = n->assigns.n ? calloc ((size_t) n->assigns.n, sizeof *save) : 0;
    for (i = 0; i < n->assigns.n; i++)
      {
	char *eq = strchr (n->assigns.v[i], '='), *val;
	struct var *v;
	if (eq > n->assigns.v[i] && (eq[-1] == '+' || strchr (n->assigns.v[i], '[') != 0) && strchr (n->assigns.v[i], '[') < eq) { do_assign (n->assigns.v[i]); save[i].name = strdup ("_"); continue; }
	if (eq > n->assigns.v[i] && eq[-1] == '+') { do_assign (n->assigns.v[i]); save[i].name = strdup ("_"); continue; }
	save[i].name = strndup (n->assigns.v[i], (size_t) (eq - n->assigns.v[i]));
	v = find_var (save[i].name);
	save[i].had = v && v->value; save[i].old = v && v->value ? strdup (v->value) : 0; save[i].exported = v ? v->exported : 0;
	val = expand_str (eq + 1);
	export_var (save[i].name, val);
	free (val);
      }
    if (!strcmp (argv.v[0], "command") && argv.n > 1)
      {
	if (!strcmp (argv.v[1], "-v"))
	  {
	    int k;
	    for (k = 2; k < argv.n; k++) { char *p = find_builtin (argv.v[k]) || find_func (argv.v[k]) ? strdup (argv.v[k]) : find_in_path (argv.v[k]); if (p) { outs (p); outs ("\n"); free (p); } else r = 1; }
	    goto restore;
	  }
	memmove (argv.v, argv.v + 1, (size_t) argv.n * sizeof *argv.v); argv.n--;
      }
    if (!strcmp (argv.v[0], "exec"))
      {
	if (argv.n > 1) { memmove (argv.v, argv.v + 1, (size_t) argv.n * sizeof *argv.v); argv.n--; r = 0; goto run_prog; }
	r = 0; keep_io = 1; goto restore;			/* exec with only redirections: they stay */
      }
    fn = find_func (argv.v[0]);
    if (fn) { r = call_function (fn, argv.n, argv.v); goto restore; }
    b = find_builtin (argv.v[0]);
    if (b && b->special == 2 && program_exists (argv.v[0])) b = 0;	/* the real utility (GNU coreutils) is there */
    if (b && b->fn)
      {
	sink_open (&cur_out);
	r = b->fn (argv.n, argv.v);
	goto restore;
      }
  run_prog:
    {
      struct sink out = cur_out;
      r = run_external (&argv, &out, cur_in_path, &cur_err);
      if (r == 127) { /* a command that RISC OS did not know */ }
      if (!strcmp (argv.v[0], "exec")) {}
    }
  restore:
    for (i = 0; i < n->assigns.n; i++)
      {
	struct var *v = find_var (save[i].name);
	if (v) { if (save[i].had) set_var (save[i].name, save[i].old); else unset_var (save[i].name); v->exported = save[i].exported; if (!v->exported) unsetenv (save[i].name); }
	free (save[i].name); free (save[i].old);
      }
    free (save);
  }
  if (keep_io) { free (saved.tmp.v); } else restore_io (&saved);
  (void) status_of_cmd;
  return r;
}

/* run a node with its standard output in a file / input from a file */
static int exec_with_io (struct node *n, const char *out_path, const char *in_path, struct sink *outer_out)
{
  struct saved_io s;
  int r;
  save_io (&s);
  if (out_path) { FILE *t = fopen (out_path, "w"); if (t) fclose (t); cur_out.kind = K_PATH; cur_out.path = (char *) out_path; cur_out.append = 0; cur_out.fp = 0; cur_out.buf = 0; }
  else if (outer_out) cur_out = *outer_out;
  if (in_path) { cur_in_path = (char *) in_path; in_gen++; }
  r = exec_node (n);
  sink_close (&cur_out);
  if (cur_in_path != s.in_path) { if (cur_in_fp) fclose (cur_in_fp); cur_in_fp = s.in_fp; }
  cur_out = s.out; cur_err = s.err; cur_in_path = s.in_path; err_null = s.err_null;
  free (s.tmp.v);
  return r;
}

static void collect_pipeline (struct node *n, struct node **stages, int *count)
{
  if (n->type == N_PIPE) { collect_pipeline (n->a, stages, count); stages[(*count)++] = n->b; }
  else stages[(*count)++] = n;
}

/* the number of lines that the stage wants, when it is  head  with only -n N, -N or nothing: else -1 */
static long head_lines (struct node *st)
{
  int i;
  long n = 10;
  if (!st || st->type != N_SIMPLE || st->words.n < 1 || strcmp (st->words.v[0], "head") || st->redirs) return -1;
  for (i = 1; i < st->words.n; i++)
    {
      const char *a = st->words.v[i];
      if (!strcmp (a, "-n") && i + 1 < st->words.n && isdigit ((unsigned char) st->words.v[i + 1][0])) { n = atol (st->words.v[++i]); }
      else if (!strncmp (a, "-n", 2) && isdigit ((unsigned char) a[2])) n = atol (a + 2);
      else if (a[0] == '-' && isdigit ((unsigned char) a[1])) n = atol (a + 1);
      else return -1;
      if (strspn (a, "-n0123456789") != strlen (a) && strcmp (a, "-n") && !isdigit ((unsigned char) a[0])) return -1;
    }
  return n;
}

static int exec_pipe (struct node *n)
{
  struct node *stages[64];
  int count = 0, i, r = 0, failed = 0;
  char *prev = 0;
  collect_pipeline (n, stages, &count);
  for (i = 0; i < count; i++)
    {
      char *outf = 0;
      if (i < count - 1) outf = scrap_name ();
      if (outf && (pipe_limit = head_lines (stages[i + 1])) >= 0) { pipe_limit_path = outf; pipe_lines = 0; broken_pipe = 0; } else pipe_limit_path = 0;
      r = exec_with_io (stages[i], outf, prev, i == count - 1 ? &cur_out : 0);
      if (pipe_limit_path && broken_pipe && !r) r = 141;
      pipe_limit = -1; pipe_limit_path = 0; broken_pipe = 0;
      if (r && opt_pipefail) failed = r;
      if (prev) { remove (prev); free (prev); }
      prev = outf;
      if (exit_flag) break;
    }
  if (prev) { remove (prev); free (prev); }
  if (opt_pipefail && failed) return failed;
  return r;
}

static int exec_subshell (struct node *n)
{
  /* ( list ): the variables, the folder and the positional parameters are the ones from before afterwards; exit ends the group only */
  struct var *saved_vars = 0, *v;
  char cwd[1024];
  char **sp = pos_params; int snp = npos;
  struct func *sf = funcs;
  int r;
  for (v = vars; v; v = v->next)
    {
      struct var *c = calloc (1, sizeof *c);
      c->name = strdup (v->name); c->value = v->value ? strdup (v->value) : 0; c->exported = v->exported; c->readonly = v->readonly;
      c->next = saved_vars; saved_vars = c;
    }
  if (!getcwd (cwd, sizeof cwd)) cwd[0] = 0;
  r = exec_node (n->a);
  if (exit_flag) { exit_flag = 0; r = exit_code; }
  if (cwd[0]) chdir (cwd);
  for (v = vars; v; v = v->next)
    {
      struct var *c;
      for (c = saved_vars; c; c = c->next) if (!strcmp (c->name, v->name)) break;
      if (c) { if (c->value) { if (!v->value || strcmp (v->value, c->value)) set_var (v->name, c->value); } else unset_var (v->name); }
      else unset_var (v->name);
    }
  while (saved_vars) { struct var *c = saved_vars; saved_vars = c->next; free (c->name); free (c->value); free (c); }
  pos_params = sp; npos = snp; funcs = sf;
  return r;
}

static int match_case (struct node *n, const char *word)
{
  struct caseitem *it;
  int i;
  for (it = n->items; it; it = it->next)
    for (i = 0; i < it->pats.n; i++)
      {
	char *pat = expand_pattern (it->pats.v[i]);
	int m = fnmatch (pat, word, 0) == 0;
	free (pat);
	if (m) return (it->body ? exec_node (it->body) : 0) + 1000000;
      }
  return -1;
}

/* ---- [[ expression ]]: no word splitting or pathname expansion of the operands; == != match a pattern; =~ is an extended regular expression; && || ! ( ) ---- */
struct dbl { char **w; int n, i; int err; };
static int dbl_or (struct dbl *d, int live);
static int dbl_binop (const char *s)
{
  static const char *ops[] = { "==", "=", "!=", "<", ">", "=~", "-eq", "-ne", "-lt", "-le", "-gt", "-ge", "-nt", "-ot", "-ef", 0 };
  int k;
  for (k = 0; ops[k]; k++) if (!strcmp (s, ops[k])) return 1;
  return 0;
}
static int dbl_unop (const char *s)
{
  return s[0] == '-' && s[1] && !s[2] && strchr ("zneEfdrwxsLhbcpSgukOG", s[1]);
}
static int dbl_primary (struct dbl *d, int live)
{
  int r;
  if (d->i >= d->n) { d->err = 1; return 0; }
  if (!strcmp (d->w[d->i], "(")) { d->i++; r = dbl_or (d, live); if (d->i < d->n && !strcmp (d->w[d->i], ")")) d->i++; else d->err = 1; return r; }
  if (!strcmp (d->w[d->i], "!")) { d->i++; return !dbl_primary (d, live); }
  if (dbl_unop (d->w[d->i]) && d->i + 1 < d->n && strcmp (d->w[d->i + 1], "&&") && strcmp (d->w[d->i + 1], "||") && strcmp (d->w[d->i + 1], ")")
      && !(d->i + 2 < d->n && dbl_binop (d->w[d->i + 1])))
    {
      const char *op = d->w[d->i];
      char *arg;
      d->i += 2;
      if (!live) return 0;
      arg = expand_str (d->w[d->i - 1]);
      if (op[1] == 'z') r = !*arg;
      else if (op[1] == 'n') r = *arg != 0;
      else if (op[1] == 'e' || op[1] == 'E') { struct stat st; r = stat (arg, &st) == 0; }
      else { char *av[4]; av[0] = "test"; av[1] = (char *) op; av[2] = arg; av[3] = 0; r = bi_test (3, av) == 0; }
      free (arg);
      return r;
    }
  if (d->i + 2 < d->n + 0 && dbl_binop (d->w[d->i + 1]))
    {
      const char *a = d->w[d->i], *op = d->w[d->i + 1], *b = d->w[d->i + 2];
      char *l, *rr;
      d->i += 3;
      if (!live) return 0;
      l = expand_str (a);
      if (!strcmp (op, "==") || !strcmp (op, "=") || !strcmp (op, "!="))
	{
	  char *pat = expand_pattern (b);
	  r = fnmatch (pat, l, 0) == 0;
	  if (op[0] == '!') r = !r;
	  free (pat);
	}
      else if (!strcmp (op, "=~"))
	{
	  regex_t re;
	  rr = expand_str (b);
	  r = regcomp (&re, rr, REG_EXTENDED) == 0 && regexec (&re, l, 0, 0, 0) == 0;
	  free (rr);
	}
      else if (!strcmp (op, "<")) { rr = expand_str (b); r = strcmp (l, rr) < 0; free (rr); }
      else if (!strcmp (op, ">")) { rr = expand_str (b); r = strcmp (l, rr) > 0; free (rr); }
      else { char *av[5]; rr = expand_str (b); av[0] = "test"; av[1] = l; av[2] = (char *) op; av[3] = rr; av[4] = 0; r = bi_test (4, av) == 0; free (rr); }
      free (l);
      return r;
    }
  {
    char *w;
    d->i++;
    if (!live) return 0;
    w = expand_str (d->w[d->i - 1]);
    r = *w != 0;
    free (w);
    return r;
  }
}
static int dbl_and (struct dbl *d, int live)
{
  int r = dbl_primary (d, live);
  while (d->i < d->n && !strcmp (d->w[d->i], "&&")) { d->i++; r = dbl_primary (d, live && r) && r; }
  return r;
}
static int dbl_or (struct dbl *d, int live)
{
  int r = dbl_and (d, live);
  while (d->i < d->n && !strcmp (d->w[d->i], "||")) { d->i++; { int b = dbl_and (d, live && !r); r = r || b; } }
  return r;
}
static int exec_dbracket (struct node *n)
{
  struct dbl d;
  int r;
  d.w = n->words.v; d.n = n->words.n; d.i = 0; d.err = 0;
  r = dbl_or (&d, 1);
  if (d.err || d.i < d.n) { errf ("sh: [[: syntax error in expression\n"); return 2; }
  return r ? 0 : 1;
}

static int exec_node (struct node *n)
{
  int r = 0;
  struct saved_io s;
  if (!n || exit_flag || broken_pipe) return last_status;
  switch (n->type)
    {
    case N_SIMPLE:
      r = exec_simple (n);
      last_status = r;
      if (r && trap_err && !in_condition) run_err_trap ();
      if (any_trap) run_pending_traps ();
      if (r && opt_e && !in_condition && !exit_flag) { exit_code = r; exit_flag = 1; }
      return r;
    case N_PIPE:
      r = exec_pipe (n);
      last_status = r;
      if (r && trap_err && !in_condition) run_err_trap ();
      if (any_trap) run_pending_traps ();
      if (r && opt_e && !in_condition && !exit_flag) { exit_code = r; exit_flag = 1; }
      return r;
    case N_DBRACKET:
      r = exec_dbracket (n);
      last_status = r;
      if (r && opt_e && !in_condition && !exit_flag) { exit_code = r; exit_flag = 1; }
      return r;
    case N_NOT:
      in_condition++;
      r = exec_node (n->a);
      in_condition--;
      r = !r; last_status = r;
      return r;
    case N_LIST:
      r = exec_node (n->a);
      if (exit_flag || brk_n || cont || ret_flag) return r;
      return exec_node (n->b);
    case N_BG:
      bg_pid = 100 + (bg_pid % 900) + 1;
      r = exec_node (n->a);			/* run now, to its end */
      job_add (bg_pid, r);
      last_status = 0;
      return 0;
    case N_AND:
      in_condition++; r = exec_node (n->a); in_condition--;
      if (exit_flag || r != 0) { last_status = r; if (r && opt_e && !in_condition && !exit_flag && n->a->type != N_AND) { } return r; }
      return exec_node (n->b);
    case N_OR:
      in_condition++; r = exec_node (n->a); in_condition--;
      if (exit_flag || r == 0) return r;
      return exec_node (n->b);
    case N_GROUP:
    case N_SUBSHELL:
      save_io (&s);
      if (apply_redirs (n->redirs, &s)) { restore_io (&s); return 1; }
      r = n->type == N_GROUP ? exec_node (n->a) : exec_subshell (n);
      restore_io (&s);
      last_status = r;
      if (r && n->type == N_SUBSHELL && trap_err && !in_condition) run_err_trap ();
      return r;
    case N_IF:
      save_io (&s);
      if (apply_redirs (n->redirs, &s)) { restore_io (&s); return 1; }
      in_condition++; r = exec_node (n->a); in_condition--;
      if (exit_flag) { restore_io (&s); return r; }
      if (r == 0) r = exec_node (n->b);
      else if (n->c) r = exec_node (n->c);
      else r = 0;
      restore_io (&s);
      last_status = r;
      return r;
    case N_WHILE:
    case N_UNTIL:
      save_io (&s);
      if (apply_redirs (n->redirs, &s)) { restore_io (&s); return 1; }
      r = 0;
      for (;;)
	{
	  int c;
	  in_condition++; c = exec_node (n->a); in_condition--;
	  if (exit_flag || ret_flag || broken_pipe) break;
	  if ((n->type == N_WHILE) ? c != 0 : c == 0) break;
	  r = exec_node (n->b);
	  if (exit_flag || ret_flag || broken_pipe) break;
	  if (brk_n) { brk_n--; break; }
	  if (cont) { if (--cont) break; }
	}
      restore_io (&s);
      last_status = r;
      return r;
    case N_FOR:
      {
	list items = { 0, 0, 0 };
	int i;
	if (n->has_in) for (i = 0; i < n->words.n; i++) expand_word (n->words.v[i], &items);
	else for (i = 0; i < npos; i++) l_add (&items, strdup (pos_params[i]));
	save_io (&s);
	if (apply_redirs (n->redirs, &s)) { restore_io (&s); return 1; }
	r = 0;
	for (i = 0; i < items.n; i++)
	  {
	    set_var (n->name, items.v[i]);
	    r = exec_node (n->a);
	    if (exit_flag || ret_flag) break;
	    if (brk_n) { brk_n--; break; }
	    if (cont) { if (--cont) break; }
	  }
	restore_io (&s);
	last_status = r;
	return r;
      }
    case N_CASE:
      {
	char *word = expand_str (n->words.v[0]);
	save_io (&s);
	if (apply_redirs (n->redirs, &s)) { restore_io (&s); free (word); return 1; }
	r = match_case (n, word);
	r = r < 0 ? 0 : r - 1000000;
	free (word);
	restore_io (&s);
	last_status = r;
	return r;
      }
    case N_FUNC:
      {
	struct func *f = calloc (1, sizeof *f);
	f->name = strdup (n->name); f->body = n->a; f->next = funcs; funcs = f;
	last_status = 0;
	return 0;
      }
    }
  return r;
}

/* ------------------------------------------------------------------------------------------------------ running text */
static int run_parser (struct parser *p)
{
  int r = 0;
  for (;;)
    {
      struct node *n;
      if (exit_flag) break;
      skip_nl (p);
      if (peek (p) == T_EOF) break;
      n = parse_list (p, 1);
      if (!n) { if (peek (p) == T_EOF) break; if (peek (p) == T_NL) { next (p); continue; } syntax (p, "unexpected token"); }
      if (peek (p) == T_NL) next (p);
      {
	clock_t t0 = profile ? clock () : 0;
	r = opt_n ? 0 : exec_node (n);			/* -n: read the commands, do not run them */
	if (profile && (clock () - t0) * 100 / CLOCKS_PER_SEC >= 50) fprintf (stderr, "sh: profile: top-level command ending at line %zu took %ld cs\n", p->pos ? (size_t) 0 : (size_t) 0, (long) ((clock () - t0) * 100 / CLOCKS_PER_SEC));
      }
      brk_n = cont = 0;
      if (ret_flag) break;
    }
  return r;
}
static int exec_string (const char *text, const char *name)
{
  struct parser p;
  int r;
  memset (&p, 0, sizeof p);
  p.line = 1;
  p.s = text; p.len = strlen (text); p.name = name; p.pend_tail = &p.pending;
  r = run_parser (&p);
  free (p.word);
  return r;
}
static int run_file (const char *path, int pass_args, int argc, char **argv)
{
  FILE *f = fopen (path, "rb");
  str b; char buf[4096]; size_t n; int r;
  char **sp = pos_params; int snp = npos;
  if (!f) { errf ("sh: %s: %s\n", path, strerror (errno)); return 127; }
  sb_init (&b); sb_adds (&b, "");
  while ((n = fread (buf, 1, sizeof buf, f)) > 0) sb_add (&b, buf, n);
  fclose (f);
  if (pass_args) set_positional (argc, argv);
  r = exec_string (b.s, path);
  if (pass_args) { pos_params = sp; npos = snp; }
  sb_free (&b);
  if (ret_flag) ret_flag = 0;
  return r;
}

/* $( ... ): the output of the commands, without the newlines at the end */
static char *capture (const char *text)
{
  str b; struct sink s; struct saved_io sv; char *r;
  int save_status = last_status;
  sb_init (&b); sb_adds (&b, "");
  save_io (&sv);
  s.kind = K_BUF; s.buf = &b; s.path = 0; s.fp = 0; s.append = 0;
  cur_out = s;
  exec_string (text, "command substitution");
  cur_out = sv.out; cur_err = sv.err; cur_in_path = sv.in_path; err_null = sv.err_null;
  free (sv.tmp.v);
  while (b.n > 0 && b.s[b.n - 1] == '\n') b.s[--b.n] = 0;
  r = sb_take (&b);
  last_status = save_status;
  return r;
}

static void run_exit_trap (void)
{
  if (trap_exit) { char *t = trap_exit; trap_exit = 0; exit_flag = 0; exec_string (t, "trap"); free (t); }
}

int main (int argc, char **argv)
{
  int i = 1, r = 0;
  const char *cmd = 0;
  char cwd[1024];
  char **env;
  arg0 = argv[0];
  profile = getenv ("SH_PROFILE") != 0;
  self_path = strdup (argv[0]);
  for (env = environ; env && *env; env++)
    {
      char *eq = strchr (*env, '=');
      if (eq) { char *name = strndup (*env, (size_t) (eq - *env)); struct var *v; set_var (name, eq + 1); v = find_var (name); if (v) v->exported = 1; free (name); }
    }
  if (!get_var ("IFS")) set_var ("IFS", " \t\n");
  if (!get_var ("PATH"))
    {
      /* the programs of GNU coreutils and the like live in an application folder that its !Boot names in a system variable: CoreUtils$Dir */
      static const char *const dirs[] = { "CoreUtils$Dir", "GNU$Dir", 0 };
      str p; int k;
      char probe[256];
      sb_init (&p); sb_adds (&p, ".:/bin:/usr/bin");
      for (k = 0; dirs[k]; k++)
	{
	  unsigned long r[10] = { (unsigned long) dirs[k], (unsigned long) probe, sizeof probe - 1, 0, 0 };
	  if (!swi_x (0x23, r)) { sb_adds (&p, ":/<"); sb_adds (&p, dirs[k]); sb_addc (&p, '>'); }
	}
      set_var ("PATH", p.s);
      sb_free (&p);
    }
  if (!get_var ("HOME")) set_var ("HOME", "/");
  if (getcwd (cwd, sizeof cwd)) set_var ("PWD", cwd);
  for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++)
    {
      const char *c;
      if (!strcmp (argv[i], "--")) { i++; break; }
      for (c = argv[i] + 1; *c; c++)
	{
	  if (*c == 'e') opt_e = 1; else if (*c == 'x') opt_x = 1; else if (*c == 'u') opt_u = 1; else if (*c == 'f') opt_f = 1; else if (*c == 'n') opt_n = 1;
	  else if (*c == 'c') { if (i + 1 >= argc) die ("-c needs a command"); cmd = argv[++i]; }
	  else if (*c == 's' || *c == 'i' || *c == 'l') ;
	  else die ("unknown option -%c", *c);
	}
    }
  if (cmd)
    {
      if (i < argc) { arg0 = argv[i]; i++; }
      set_positional (argc - i, argv + i);
      r = exec_string (cmd, "-c");
    }
  else if (i < argc)
    {
      arg0 = argv[i];
      set_positional (argc - i - 1, argv + i + 1);
      r = run_file (argv[i], 0, 0, 0);
    }
  else
    {
      str b; char buf[4096]; size_t n;
      set_positional (0, argv);
      sb_init (&b); sb_adds (&b, "");
      while ((n = fread (buf, 1, sizeof buf, stdin)) > 0) sb_add (&b, buf, n);
      r = exec_string (b.s, "stdin");
    }
  if (exit_flag) r = exit_code;
  last_status = r;
  run_exit_trap ();
  if (exit_flag) r = exit_code;
  fflush (stdout);
  return r & 255;
}
