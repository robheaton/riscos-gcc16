/* termcap.c - the termcap interface (tgetent, tgetflag, tgetnum, tgetstr, tgoto, tputs) over a built-in database of two terminals: "riscos" (the VDU drivers: VDU 31 x y to move the cursor, VDU 12 clear, VDU 17 for the
   colours that make standout, the cursor keys as the keyboard returns them) and "dumb".  Every other terminal name gets the "riscos" entry (a program that finds TERM set to xterm still draws with VDU codes).  The
   size of the screen (li, co) is read from the VDU variables.  There is no clear-to-end-of-line in the VDU drivers: "ce" and "cd" are the byte sequences 0xFF 0x1B 0x01 / 0x02 that write() to the console (riscos_syscalls.c) turns into spaces and a cursor move.  The SDK of RISC OS 64. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);

char PC;
char *UP, *BC;
short ospeed;

static const char *const db[] = {
  "riscos|riscos64|RISC OS text screen:am:bs:cl=^L:ho=^^:cm=^_%r%.%.:up=^K:do=^J:le=^H:nd=^I:ce=\\377^[^A:cd=\\377^[^B:cr=^M:nl=^J:sf=^J:sr=^K:sr=^K:bl=^G:so=^Q^D^Q\\207:se=^Q^G^Q\\200:us=^Q^D^Q\\207:ue=^Q^G^Q\\200:md=^Q^D^Q\\207:me=^Q^G^Q\\200:ku=\\217:kd=\\216:kl=\\214:kr=\\215:kP=\\237:kN=\\236:kh=^^:@7=\\213:li#25:co#80:",
  "dumb|80-column dumb tty:am:bl=^G:co#80:cr=^M:do=^J:sf=^J:",
  0 };

static char entry[2048];
static int loaded;

static int decode (const char *s, char *out, size_t n)	/* the escapes of a termcap string: \E \n \r \t \b \f \\ \^ \: \NNN, ^X (a string cannot hold a NUL: the entries avoid it) */
{
  size_t k = 0;
  while (*s && *s != ':' && k + 1 < n)
    {
      int c = (unsigned char) *s++;
      if (c == '\\')
	{
	  c = (unsigned char) *s++;
	  switch (c)
	    {
	    case 'E': case 'e': c = 27; break;
	    case 'n': c = '\n'; break; case 'r': c = '\r'; break; case 't': c = '\t'; break; case 'b': c = '\b'; break; case 'f': c = '\f'; break;
	    case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7':
	      { int v = c - '0', i = 1; while (i < 3 && *s >= '0' && *s <= '7') { v = v * 8 + (*s++ - '0'); i++; } c = v & 255; break; }
	    default: break;
	    }
	}
      else if (c == '^' && *s) { c = *s++; c = c == '?' ? 127 : (c & 31); }
      out[k++] = (char) c;
    }
  out[k] = 0;
  return (int) k;
}

int tgetent (char *bp, const char *name)
{
  int i;
  const char *e = 0;
  (void) bp;
  if (!name) name = "riscos";
  for (i = 0; db[i] && !e; i++)
    {
      const char *p = db[i], *end = strchr (p, ':');
      size_t l = (size_t) (end - p);
      char names[256];
      char *tok;
      if (l >= sizeof names) l = sizeof names - 1;
      memcpy (names, p, l); names[l] = 0;
      for (tok = strtok (names, "|"); tok; tok = strtok (0, "|")) if (!strcmp (tok, name)) { e = db[i]; break; }
    }
  if (!e) e = db[0];
  strncpy (entry, e, sizeof entry - 1); entry[sizeof entry - 1] = 0;
  loaded = 1;
  return 1;
}

static const char *find (const char *id, char kind)	/* kind: '=' string, '#' number, 0 flag */
{
  const char *p = entry;
  if (!loaded) tgetent (0, getenv ("TERM"));
  p = strchr (entry, ':');
  while (p)
    {
      p++;
      if (p[0] == id[0] && p[1] == id[1])
	{
	  if (kind == 0 && (p[2] == ':' || !p[2])) return p;
	  if (kind && p[2] == kind) return p + 3;
	}
      p = strchr (p, ':');
    }
  return 0;
}

int tgetflag (const char *id) { return find (id, 0) != 0; }

int tgetnum (const char *id)
{
  const char *p;
  if (!strcmp (id, "co") || !strcmp (id, "li"))
    {
      unsigned long in[2] = { !strcmp (id, "co") ? 1 : 2, (unsigned long) -1 }, out[2] = { 0, 0 };
      unsigned long r[10] = { (unsigned long) in, (unsigned long) out };
      /* OS_ReadVduVariables takes a list of 32-bit words */
      unsigned in32[2] = { !strcmp (id, "co") ? 1u : 2u, 0xFFFFFFFFu }, out32[2] = { 0, 0 };
      r[0] = (unsigned long) in32; r[1] = (unsigned long) out32;
      (void) in; (void) out;
      if (!swi_x (0x31, r) && out32[0] < 1000) return (int) out32[0] + 1;	/* ScrRCol, ScrBRow: the last column / row */
    }
  p = find (id, '#');
  return p ? atoi (p) : -1;
}

char *tgetstr (const char *id, char **area)
{
  const char *p = find (id, '=');
  char tmp[256];
  int n;
  if (!p) return 0;
  n = decode (p, tmp, sizeof tmp);
  if (area && *area) { char *r = *area; memcpy (r, tmp, (size_t) n + 1); *area += n + 1; return r; }
  { char *r = malloc ((size_t) n + 1); if (r) memcpy (r, tmp, (size_t) n + 1); return r; }
}

/* tgoto: %d %2 %3 %. %+x %>xy %r %i %n %B %D %% (two arguments: column and row; cm wants the row first, as in termcap) */
char *tgoto (const char *cap, int col, int row)
{
  static char buf[64];
  int args[2] = { row, col }, ai = 0;
  size_t k = 0;
  const char *s = cap;
  if (!cap) return "OOPS";
  while (*s && k + 8 < sizeof buf)
    {
      if (*s != '%') { buf[k++] = *s++; continue; }
      s++;
      switch (*s++)
	{
	case 'd': k += (size_t) sprintf (buf + k, "%d", args[ai++]); break;
	case '2': k += (size_t) sprintf (buf + k, "%02d", args[ai++]); break;
	case '3': k += (size_t) sprintf (buf + k, "%03d", args[ai++]); break;
	case '.': buf[k++] = (char) args[ai++]; break;
	case '+': buf[k++] = (char) (args[ai++] + *s++); break;
	case 'r': { int t = args[0]; args[0] = args[1]; args[1] = t; break; }
	case 'i': args[0]++; args[1]++; break;
	case 'n': args[0] ^= 0140; args[1] ^= 0140; break;
	case 'B': args[ai] = ((args[ai] / 10) << 4) + args[ai] % 10; break;
	case 'D': args[ai] = args[ai] - 2 * (args[ai] & 15); break;
	case '%': buf[k++] = '%'; break;
	default: break;
	}
    }
  buf[k] = 0;
  return buf;
}

int tputs (const char *str, int affcnt, int (*putc_) (int))
{
  (void) affcnt;
  if (!str) return 0;
  while (*str == '*' || (*str >= '0' && *str <= '9') || *str == '.') str++;	/* the padding prefix is skipped: no delays here */
  while (*str) putc_ ((unsigned char) *str++);
  return 0;
}
