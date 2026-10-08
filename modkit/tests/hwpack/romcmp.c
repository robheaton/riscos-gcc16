/* romcmp.c - the ROM modules Squash, MimeMap and DrawFile against the same modules built from the RISC OS Open sources with GCC 16 and the module kit (pack/romcmp37).  One program: it is run once with the modules
   of the ROM active and once with the builds loaded over them; each run writes a file of result lines, the lines of the two files must be the same, and the program checks by itself what it can (the round trip of
   the compression, the answers of MimeMap for a mapping file whose content it knows, the transformed box of a Draw file).
     RMRun RomCmp run <tag> <result file> [squash] [mime <TestMap> <SysMap>] [draw] [render] [quick]     (default: squash mime draw; the files of mime are the two mapping files of the pack)
     RMRun RomCmp diff <result A> <result B>      the lines that are not the same (lines that start with # say what and where; they are not compared); exit code 0 = no difference
     RMRun RomCmp summary <result file>            the FAIL lines and the count of checks of a run (the Obey file prints it again: a run closes the spool)
     RMRun RomCmp need <module file> <title>      checks a module image BEFORE RMLoad (the header words, the title, the relocation table): an error stops the Obey file when it is wrong
     RMRun RomCmp try <command>                   runs a command and prints the error that it gives (the Obey file goes on)
     RMRun RomCmp info                            the modules that are active (help string, where) and the free RMA
     RMRun RomCmp savevar <name> / restorevar <name>    keeps a system variable (Inet$MimeMappings) in the scrap directory / puts it back
   Exit code 0: no check failed.  1: a check failed (a line FAIL in the result file and on the screen), or the two files differ.  2: wrong arguments or a file that cannot be written.
   Nothing here is specific to the machine: tests/sim-romcmp.py runs the same program on the interpreter against the GCC builds. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <kernel.h>
#include <swis.h>

#define RC_SQ_COMPRESS    0x42700
#define RC_SQ_DECOMPRESS  0x42701
#define RC_MIME_TRANSLATE 0x50B00
#define RC_DF_RENDER      0x45540
#define RC_DF_BBOX        0x45541
#define RC_DF_DECLARE     0x45542

_kernel_oserror *rc_init (const char *tail, int podule_base, void *pw) { (void) tail; (void) podule_base; (void) pw; return 0; }

/* ------------------------------------------------------------------------------------------------ results */
static FILE *out;
static unsigned n_checks, n_fail;
static int quick;

static void emit (const char *fmt, ...) __attribute__ ((format (__printf__, 1, 2)));
static void emit (const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  vfprintf (out, fmt, ap);
  va_end (ap);
  fputc ('\n', out);
}

static void fail (const char *fmt, ...) __attribute__ ((format (__printf__, 1, 2)));
static void fail (const char *fmt, ...)
{
  char b[400];
  va_list ap;
  va_start (ap, fmt);
  vsnprintf (b, sizeof b, fmt, ap);
  va_end (ap);
  fprintf (out, "FAIL %s\n", b);
  printf ("FAIL %s\n", b);
  n_fail++;
}
#define CHECK(cond, ...) do { n_checks++; if (!(cond)) fail (__VA_ARGS__); } while (0)

static const char *etext (const _kernel_oserror *e)                  /* "none" or "&NUMBER 'text'" (two buffers: a printf can ask for two) */
{
  static char b[2][220];
  static int k;
  char *p;
  unsigned i;
  if (!e) return "none";
  p = b[k ^= 1];
  snprintf (p, sizeof b[0], "&%X '%.150s'", (unsigned) e->errnum, e->errmess);
  for (i = 0; p[i]; i++) if ((unsigned char) p[i] < 32 || (unsigned char) p[i] > 126) p[i] = '?';
  return p;
}
static const char *enum_ (const _kernel_oserror *e)                  /* "none" or "!&NUMBER" */
{
  static char b[2][24];
  static int k;
  char *p = b[k ^= 1];
  if (!e) return "ok";
  snprintf (p, sizeof b[0], "!&%X", (unsigned) e->errnum);
  return p;
}

static uint32_t crc32 (uint32_t crc, const void *p, size_t n)
{
  const uint8_t *b = p;
  int k;
  crc = ~crc;
  while (n--)
    {
      crc ^= *b++;
      for (k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1));
    }
  return ~crc;
}

static uint32_t rng;
static uint32_t rnd (void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

static const char *scrap (void)
{
  const char *s = getenv ("Wimp$ScrapDir");
  return s && *s ? s : "<Wimp$ScrapDir>";
}

static char *slurp (const char *path, size_t *len)
{
  FILE *f = fopen (path, "rb");
  char *d;
  size_t n = 0, cap = 4096, r;
  if (!f) return NULL;
  d = malloc (cap + 1);
  while (d && (r = fread (d + n, 1, cap - n, f)) > 0)
    {
      n += r;
      if (n == cap)
        {
          char *nd = realloc (d, cap * 2 + 1);
          if (!nd) { free (d); d = NULL; break; }
          d = nd; cap *= 2;
        }
    }
  fclose (f);
  if (!d) return NULL;
  d[n] = 0;
  if (len) *len = n;
  return d;
}

static void gstrans (const char *in, char *dst, size_t cap)          /* <Var> in an argument (the OS may or may not have done it) */
{
  unsigned len = 0;
  if (!strchr (in, '<') || _swix (OS_GSTrans, _INR (0, 2) | _OUT (2), in, dst, (unsigned) (cap - 1), &len) != NULL)
    {
      snprintf (dst, cap, "%s", in);
      return;
    }
  dst[len < cap - 1 ? len : cap - 1] = 0;
}

/* runs a command with *Spool on a file of the scrap directory and returns what it printed (carriage returns removed); *err is the error that the command gave */
static char *capture (const char *cmd, const _kernel_oserror **err)
{
  char path[300], c[400], *t, *p, *q;
  const _kernel_oserror *e;
  snprintf (path, sizeof path, "%s.RomCmpCap", scrap ());
  snprintf (c, sizeof c, "Spool %s", path);
  e = _swix (OS_CLI, _IN (0), c);
  if (!e) e = _swix (OS_CLI, _IN (0), cmd);
  else (void) _swix (OS_CLI, _IN (0), cmd);
  _swix (OS_CLI, _IN (0), "Spool");
  *err = e;
  t = slurp (path, NULL);
  remove (path);
  if (!t) return NULL;
  for (p = q = t; *p; p++) if (*p != '\r') *q++ = *p;
  *q = 0;
  return t;
}

static void emit_capture (const char *label, const char *cmd, int skip_first)   /* the lines of a command, one result line each (skip_first: the first line goes to a # line: it has the version) */
{
  const _kernel_oserror *e;
  char *t = capture (cmd, &e), *p, *nl;
  int line = 0;
  emit ("cmd.%s '%s' error=%s", label, cmd, etext (e));
  if (!t) { emit ("cmd.%s no output file", label); return; }
  for (p = t; *p; p = nl ? nl + 1 : p + strlen (p))
    {
      nl = strchr (p, '\n');
      if (nl) *nl = 0;
      if (line == 0 && skip_first) emit ("# cmd.%s first line: %s", label, p);
      else emit ("cmd.%s.%d %s", label, line, p);
      line++;
      if (!nl) break;
    }
  free (t);
}

/* ------------------------------------------------------------------------------------------------ the modules, and the machine */
static void modinfo (FILE *f, const char *name)
{
  unsigned base = 0, mod = 0, inst = 0, ws = 0, post = 0;
  const _kernel_oserror *e = _swix (OS_Module, _INR (0, 1) | _OUTR (1, 5), 18, name, &mod, &inst, &base, &ws, &post);
  if (e) { fprintf (f, "# module %s: %s\n", name, etext (e)); return; }
  {
    unsigned flags = 0;
    const _kernel_oserror *e2 = _swix (OS_ValidateAddress, _INR (0, 1) | _OUT (_FLAGS), base, base + 0x40, &flags);
    if (e2 || (flags & _C)) { fprintf (f, "# module %s at &%X: the header cannot be read\n", name, base); return; }
  }
  {
    const uint32_t *h = (const uint32_t *) base;
    const char *help = (const char *) (base + h[5]);
    char b[100];
    unsigned i;
    snprintf (b, sizeof b, "%.90s", help);
    for (i = 0; b[i]; i++) if ((unsigned char) b[i] < 32 || (unsigned char) b[i] > 126) b[i] = ' ';
    fprintf (f, "# module %s: help '%s' at &%X\n", name, b, base);
  }
}

static void info (FILE *f)
{
  unsigned max = 0, fr = 0;
  modinfo (f, "MimeMap");
  modinfo (f, "Squash");
  modinfo (f, "DrawFile");
  if (!_swix (OS_Module, _IN (0) | _OUTR (2, 3), 5, &max, &fr)) fprintf (f, "# RMA: largest free block %u, free %u\n", max, fr);
}

static void swi_names (void)
{
  static const struct { const char *name; unsigned num; } t[] = {
    { "MimeMap_Translate", RC_MIME_TRANSLATE }, { "Squash_Compress", RC_SQ_COMPRESS }, { "Squash_Decompress", RC_SQ_DECOMPRESS },
    { "DrawFile_Render", RC_DF_RENDER }, { "DrawFile_BBox", RC_DF_BBOX }, { "DrawFile_DeclareFonts", RC_DF_DECLARE } };
  unsigned i;
  for (i = 0; i < sizeof t / sizeof t[0]; i++)
    {
      unsigned n = 0, len = 0;
      char b[48] = "";
      const _kernel_oserror *e = _swix (0x39, _IN (1) | _OUT (0), t[i].name, &n);
      const _kernel_oserror *e2 = _swix (0x38, _INR (0, 2) | _OUT (2), t[i].num, b, (unsigned) sizeof b, &len);
      emit ("swi.%s number=&%X error=%s tostring='%s' error=%s", t[i].name, n, enum_ (e), b, enum_ (e2));
      CHECK (!e && n == t[i].num, "SWI %s is &%X, not &%X (%s)", t[i].name, n, t[i].num, etext (e));
      CHECK (!e2 && strcmp (b, t[i].name) == 0, "the name of SWI &%X is '%s', not '%s'", t[i].num, b, t[i].name);
    }
}

/* ------------------------------------------------------------------------------------------------ Squash */
static unsigned char *g_ws;
static unsigned g_wsc, g_wsd;                                            /* the work space sizes that the module gave */
#define WS_MAX (g_wsc > g_wsd ? g_wsc : g_wsd)
static void ws_prepare (unsigned size) { memset (g_ws, 0, size); memset (g_ws + size, 0xA5, 64); }
static int ws_ok (unsigned size) { unsigned k; for (k = 0; k < 64; k++) if (g_ws[size + k] != 0xA5) return 0; return 1; }
typedef struct { long produced; unsigned calls, status_mask, leftover; const _kernel_oserror *err; } sqrun_t;

/* swi: compress or decompress; extra: flag bits that go in every call (4: the fast decompression); the input is delivered cin bytes at a time (0: all), the output is taken cout bytes at a time (0: all the room);
   force_state: the first call says "more input" even when everything is there (the restartable code, not the fast code).  The unconsumed input of a call is delivered again with the next piece, as the interface says. */
static void sq_run (sqrun_t *s, int swi, unsigned extra, const uint8_t *in, unsigned n, uint8_t *ob, unsigned cap, unsigned cin, unsigned cout_, int force_state)
{
  unsigned pos_in = 0, avail, pos_out = 0, cont = 0, guard = 0;
  memset (s, 0, sizeof *s);
  avail = cin && cin < n ? cin : n;
  for (;;)
    {
      unsigned last = avail >= n && !(force_state && !cont);
      unsigned win = cap - pos_out, flags, o0, o1, o2, o3, o4, o5;
      const _kernel_oserror *e;
      if (cout_ && win > cout_) win = cout_;
      flags = extra | (cont ? 1u : 0u) | (last ? 0u : 2u);
      e = _swix (swi, _INR (0, 5) | _OUTR (0, 5), flags, g_ws, in + pos_in, avail - pos_in, ob + pos_out, win, &o0, &o1, &o2, &o3, &o4, &o5);
      if (e) { s->err = e; s->produced = -1; s->leftover = n - pos_in; return; }
      s->calls++;
      pos_in += (avail - pos_in) - o3;
      pos_out = o4 - (unsigned) (uintptr_t) ob;
      s->status_mask |= 1u << (o0 & 31);
      cont = 1;
      if (o0 == 0) break;
      if (o0 == 1)
        {
          if (last) { s->produced = -2; s->leftover = n - pos_in; return; }
          if (cin && avail < n) { avail += cin; if (avail > n) avail = n; }
        }
      else if (o0 == 2)
        {
          if (win == 0) { s->produced = -3; s->leftover = n - pos_in; return; }
        }
      else { s->produced = -5; s->leftover = n - pos_in; return; }
      if (++guard > 8u * (n + cap) + 1000u) { s->produced = -4; s->leftover = n - pos_in; return; }
    }
  s->produced = (long) pos_out;
  s->leftover = n - pos_in;
}

static const char *const words[] = { "the", "of", "and", "module", "kernel", "RISC", "OS", "squash", "compress", "table", "code", "bits", "stream", "data", "file", "type", "map", "draw", "path",
  "object", "sprite", "window", "memory", "block", "swi", "handler", "service", "call", "word", "byte", "string", "buffer", "length", "pointer", "register", "error", "number", "text", "list", "name" };

static void fill_text (uint8_t *p, unsigned n)
{
  unsigned i = 0;
  while (i < n)
    {
      const char *w = words[rnd () % (sizeof words / sizeof words[0])];
      while (*w && i < n) p[i++] = (uint8_t) *w++;
      if (i < n) p[i++] = (rnd () % 12 == 0) ? '\n' : ' ';
    }
}

static const struct { const char *name; char kind; unsigned n_full, n_quick; } sq_inputs[] = {
  { "empty", 'z', 0, 0 }, { "one", 'r', 1, 1 }, { "two", 'r', 2, 2 }, { "tobe", 'L', 24, 24 }, { "zeros", 'z', 4000, 1500 }, { "count", 'c', 3000, 1000 },
  { "text5k", 't', 5000, 1500 }, { "rand5k", 'r', 5000, 1500 }, { "runs", 'u', 20000, 3000 }, { "text30k", 't', 30000, 6000 }, { "mixed", 'm', 150000, 24000 } };

static void sq_gen (int i, uint8_t *p, unsigned n)
{
  unsigned k;
  rng = 0x9E3779B9u + (uint32_t) i * 7919u;
  switch (sq_inputs[i].kind)
    {
    case 'z': memset (p, 0, n); break;
    case 'r': for (k = 0; k < n; k++) p[k] = (uint8_t) rnd (); break;
    case 'L': memcpy (p, "TOBEORNOTTOBEORTOBEORNOT", 24); break;
    case 'c': for (k = 0; k < n; k++) p[k] = (uint8_t) k; break;
    case 't': fill_text (p, n); break;
    case 'u':
      for (k = 0; k < n; )
        {
          unsigned run = 1 + rnd () % 40, c = rnd () & 0xFF;
          while (run-- && k < n) p[k++] = (uint8_t) c;
        }
      break;
    case 'm':                                                           /* text, random bytes, zeros and counting in turn: the ratio changes, the code table fills and is cleared */
      for (k = 0; k < n; )
        {
          unsigned seg = 3000 + rnd () % 4000, j, kind = rnd () % 4;
          if (k + seg > n) seg = n - k;
          if (kind == 0) fill_text (p + k, seg);
          else if (kind == 1) for (j = 0; j < seg; j++) p[k + j] = (uint8_t) rnd ();
          else if (kind == 2) memset (p + k, 0, seg);
          else for (j = 0; j < seg; j++) p[k + j] = (uint8_t) (j * 3);
          k += seg;
        }
      break;
    }
}

#define CANARY 8
static uint8_t *canary_alloc (unsigned n)                               /* n bytes and a guard after them */
{
  uint8_t *p = malloc (n + CANARY + 8);
  if (p) memset (p + n, 0xA5, CANARY);
  return p;
}
static int canary_ok (const uint8_t *p, unsigned n)
{
  unsigned k;
  for (k = 0; k < CANARY; k++) if (p[n + k] != 0xA5) return 0;
  return 1;
}

typedef struct { const char *name; int fast, force_state; unsigned cin, cout_; } sqvar_t;

static void squash_suite (void)
{
  unsigned wsc = 0, wsd = 0, mx = 0, r1 = 0, wsall, i;
  const _kernel_oserror *e;
  unsigned before = n_fail;
  e = _swix (RC_SQ_COMPRESS, _INR (0, 1) | _OUTR (0, 1), 8, 0xFFFFFFFFu, &wsc, &r1);
  emit ("squash.wsize compress=%u max=%d error=%s", wsc, (int) r1, enum_ (e));
  e = _swix (RC_SQ_COMPRESS, _INR (0, 1) | _OUTR (0, 1), 8, 1000, &wsc, &mx);
  emit ("squash.wsize compress(1000)=%u max=%u error=%s", wsc, mx, enum_ (e));
  e = _swix (RC_SQ_COMPRESS, _INR (0, 1) | _OUTR (0, 1), 8, 0, &wsc, &mx);
  emit ("squash.wsize compress(0)=%u max=%u error=%s", wsc, mx, enum_ (e));
  e = _swix (RC_SQ_DECOMPRESS, _INR (0, 1) | _OUTR (0, 1), 8, 0xFFFFFFFFu, &wsd, &r1);
  emit ("squash.wsize decompress=%u max=%d error=%s", wsd, (int) r1, enum_ (e));
  CHECK (wsc >= 4096 && wsd >= 4096 && wsc < 1000000 && wsd < 1000000, "the work space sizes are %u and %u", wsc, wsd);
  g_wsc = wsc; g_wsd = wsd;
  wsall = (wsc > wsd ? wsc : wsd) + 64;
  g_ws = malloc (wsall + 8);
  if (!g_ws) { fail ("no memory for the work space"); return; }
  /* ---- the inputs */
  for (i = 0; i < sizeof sq_inputs / sizeof sq_inputs[0]; i++)
    {
      unsigned n = quick ? sq_inputs[i].n_quick : sq_inputs[i].n_full, cap = 64 + n + n / 2, maxout = 0, v, d, nv;
      uint8_t *x = canary_alloc (n + 1), *cs[8], *dz = canary_alloc (n + 64);
      unsigned cl[8];
      uint32_t ccrc[8], xcrc;
      sqvar_t var[8];
      sqrun_t s;
      int sref = -1;
      memset (cs, 0, sizeof cs);
      if (!x || !dz) { fail ("no memory for input %d", i); free (x); free (dz); continue; }
      sq_gen ((int) i, x, n);
      xcrc = crc32 (0, x, n);
      emit ("squash.%s input n=%u crc=%08x", sq_inputs[i].name, n, (unsigned) xcrc);
      e = _swix (RC_SQ_COMPRESS, _INR (0, 1) | _OUTR (0, 1), 8, n, &wsc, &maxout);
      /* the variants of the compression */
      nv = 0;
      var[nv++] = (sqvar_t) { "fast", 1, 0, 0, 0 };
      var[nv++] = (sqvar_t) { "state", 0, 1, 0, 0 };
      var[nv++] = (sqvar_t) { "c64x13", 0, 0, 64, 13 };
      if (n <= 300) var[nv++] = (sqvar_t) { "c1x12", 0, 0, 1, 12 };
      if (n > 2000) var[nv++] = (sqvar_t) { "c1000x1000", 0, 0, 1000, 1000 };
      for (v = 0; v < nv; v++)
        {
          /* an empty input goes to the restartable code only: the fast code (compress_store_ass, s/comp_ass) takes the first byte and subtracts 1 from the length before it looks at it, so with a length of 0 it
             compresses 4 billion bytes and writes them behind the buffer.  That is in the ROM module as well; it is not run here (the module sends everything with 3 + 3 * length / 2 <= r5 and no flag bits to it) */
          if (n == 0 && v != 1) { cl[v] = 0; ccrc[v] = 0; continue; }
          cs[v] = canary_alloc (cap);
          if (!cs[v]) { fail ("no memory for the output of input %d", i); cl[v] = 0; continue; }
          ws_prepare (g_wsc);
          sq_run (&s, RC_SQ_COMPRESS, 0, x, n, cs[v], cap, var[v].cin, var[v].cout_, var[v].force_state);
          cl[v] = s.produced > 0 ? (unsigned) s.produced : 0;
          ccrc[v] = crc32 (0, cs[v], cl[v]);
          emit ("squash.%s compress.%s result=%ld length=%u crc=%08x calls=%u statuses=%x left=%u error=%s", sq_inputs[i].name, var[v].name, s.produced, cl[v], (unsigned) ccrc[v], s.calls, s.status_mask, s.leftover, enum_ (s.err));
          CHECK (s.produced >= 0 && s.leftover == 0, "compress %s %s: result %ld, input left %u, error %s", sq_inputs[i].name, var[v].name, s.produced, s.leftover, etext (s.err));
          CHECK (canary_ok (cs[v], cap), "compress %s %s wrote beyond its buffer", sq_inputs[i].name, var[v].name);
          CHECK (ws_ok (g_wsc), "compress %s %s wrote beyond the work space of %u bytes", sq_inputs[i].name, var[v].name, g_wsc);
          if (s.produced >= 0 && !e && maxout != 0xFFFFFFFFu) CHECK (cl[v] <= maxout, "compress %s %s: %u bytes, more than the %u that the module promised", sq_inputs[i].name, var[v].name, cl[v], maxout);
          if (v == 1) sref = 1;
        }
      if (sref >= 0 && cs[1])                                           /* the restartable code must not depend on how the data is cut up */
        for (v = 2; v < nv; v++)
          if (cs[v]) CHECK (cl[v] == cl[1] && ccrc[v] == ccrc[1], "compress %s: %s gives another stream than state (%u bytes crc %08x, state %u bytes crc %08x)", sq_inputs[i].name, var[v].name, cl[v], (unsigned) ccrc[v], cl[1], (unsigned) ccrc[1]);
      if (cs[0] && cs[1]) emit ("squash.%s compress.fast-vs-state same=%s", sq_inputs[i].name, cl[0] == cl[1] && ccrc[0] == ccrc[1] ? "yes" : "no");
      /* the decompression of the streams of the fast and of the restartable code, by each variant */
      for (d = 0; d < 2 && d < nv; d++)
        {
          sqvar_t dv[5];
          unsigned ndv = 0, j;
          if (!cs[d] || !cl[d]) continue;
          if (n > 0) dv[ndv++] = (sqvar_t) { "fast", 1, 0, 0, 0 };            /* (not for an empty stream: see above) */
          dv[ndv++] = (sqvar_t) { "state", 0, 0, 0, 0 };
          dv[ndv++] = (sqvar_t) { "c64x17", 0, 0, 64, 17 };
          if (n <= 300) dv[ndv++] = (sqvar_t) { "c13x1", 0, 0, 13, 1 };
          if (n > 2000) dv[ndv++] = (sqvar_t) { "c4096x4096", 0, 0, 4096, 4096 };
          for (j = 0; j < ndv; j++)
            {
              int same;
              ws_prepare (g_wsd);
              memset (dz, 0x55, n + 64);
              sq_run (&s, RC_SQ_DECOMPRESS, dv[j].fast ? 4 : 0, cs[d], cl[d], dz, n + 64, dv[j].cin, dv[j].cout_, 0);
              same = s.produced == (long) n && memcmp (dz, x, n) == 0;
              emit ("squash.%s decompress.%s.%s result=%ld same=%s calls=%u statuses=%x left=%u error=%s", sq_inputs[i].name, d ? "state" : "fast", dv[j].name, s.produced, same ? "yes" : "no", s.calls, s.status_mask, s.leftover, enum_ (s.err));
              CHECK (same, "decompress %s of the %s stream of %s: %ld bytes instead of %u, or other bytes (error %s)", dv[j].name, d ? "state" : "fast", sq_inputs[i].name, s.produced, n, etext (s.err));
              CHECK (canary_ok (dz, n + 64), "decompress %s %s wrote beyond its buffer", sq_inputs[i].name, dv[j].name);
              CHECK (ws_ok (g_wsd), "decompress %s %s wrote beyond the work space of %u bytes", sq_inputs[i].name, dv[j].name, g_wsd);
            }
        }
      CHECK (canary_ok (x, n + 1), "the input %s was written beyond", sq_inputs[i].name);
      for (v = 0; v < 8; v++) free (cs[v]);
      free (x);
      free (dz);
      fflush (out);
    }
  /* ---- the output space that is not enough, the input that is cut short, damaged input, wrong parameters */
  {
    uint8_t *x = canary_alloc (2000), *cb = canary_alloc (4000), *dz = canary_alloc (2100);
    sqrun_t s;
    unsigned cl, k;
    if (x && cb && dz)
      {
        rng = 12345; fill_text (x, 2000);
        ws_prepare (WS_MAX);
        sq_run (&s, RC_SQ_COMPRESS, 0, x, 2000, cb, 4000, 0, 0, 1);
        cl = s.produced > 0 ? (unsigned) s.produced : 0;
        emit ("squash.edge stream length=%u", cl);
        /* compress into 11 bytes: out of output space (status 2, r5 < 12) */
        {
          unsigned o0 = 0, o2, o3, o4, o5 = 0;
          ws_prepare (WS_MAX);
          e = _swix (RC_SQ_COMPRESS, _INR (0, 5) | _OUTR (0, 5), 2, g_ws, x, 2000, dz, 11, &o0, &k, &o2, &o3, &o4, &o5);
          emit ("squash.edge compress-small-output error=%s status=%u input-left=%u output-left=%u", enum_ (e), o0, o3, o5);
          CHECK (!e && o0 == 2, "compress into 11 bytes: status %u (error %s), 2 expected", o0, etext (e));
        }
        /* decompress into too little room: status 2 and nothing beyond the buffer */
        ws_prepare (WS_MAX);
        memset (dz, 0x55, 2100);
        sq_run (&s, RC_SQ_DECOMPRESS, 0, cb, cl, dz, 100, 0, 0, 0);
        emit ("squash.edge decompress-small-output result=%ld statuses=%x", s.produced, s.status_mask);
        CHECK (canary_ok (dz, 2100) && dz[100] == 0x55, "the decompression wrote beyond the 100 bytes that it was given");
        /* the stream cut short (no more input after it): the answer is an error or the status 1, never a hang */
        for (k = 0; k < 4; k++)
          {
            unsigned cut = k == 0 ? 0 : k == 1 ? 2 : k == 2 ? cl / 2 : cl - 1;
            ws_prepare (WS_MAX);
            memset (dz, 0x55, 2100);
            sq_run (&s, RC_SQ_DECOMPRESS, 0, cb, cut, dz, 2000, 0, 0, 0);
            emit ("squash.edge decompress-cut-at-%u result=%ld statuses=%x error=%s", cut, s.produced, s.status_mask, etext (s.err));
            CHECK (canary_ok (dz, 2100) && dz[2000] == 0x55, "the decompression of a stream cut at %u wrote beyond the buffer", cut);
          }
        /* damaged: bad header byte, bad codes */
        for (k = 0; k < 3; k++)
          {
            unsigned j;
            memcpy (dz, cb, cl > 400 ? 400 : cl);
            if (k == 0) dz[2] = 0x8B;
            else if (k == 1) { rng = 777; for (j = 3; j < 200; j++) dz[j] = (uint8_t) rnd (); }
            else { dz[0] = 0; dz[1] = 0; }
            {
              uint8_t *o2b = canary_alloc (5000);
              if (o2b)
                {
                  ws_prepare (WS_MAX);
                  sq_run (&s, RC_SQ_DECOMPRESS, 0, dz, cl > 400 ? 400 : cl, o2b, 5000, 0, 0, 0);
                  emit ("squash.edge decompress-damaged-%u result=%ld statuses=%x error=%s", k, s.produced, s.status_mask, etext (s.err));
                  CHECK (canary_ok (o2b, 5000), "the decompression of damaged data %u wrote beyond the buffer", k);
                  free (o2b);
                }
            }
          }
        /* wrong parameters, addresses and SWIs */
        {
          static const unsigned bad_flags[] = { 0x10, 0x9, 0x18, 0x80000000u };
          unsigned b;
          for (b = 0; b < sizeof bad_flags / sizeof bad_flags[0]; b++)
            {
              unsigned o0 = 0, o1;
              ws_prepare (WS_MAX);
              e = _swix (RC_SQ_COMPRESS, _INR (0, 5) | _OUTR (0, 1), bad_flags[b], g_ws, x, 100, cb, 4000, &o0, &o1);
              emit ("squash.edge compress-flags-%x error=%s", bad_flags[b], etext (e));
              e = _swix (RC_SQ_DECOMPRESS, _INR (0, 5) | _OUTR (0, 1), bad_flags[b], g_ws, x, 100, cb, 4000, &o0, &o1);
              emit ("squash.edge decompress-flags-%x error=%s", bad_flags[b], etext (e));
            }
          /* a buffer that is real with a length that goes far beyond the memory: the module says "bad address" at once; if a machine did let it through, only the 100 bytes of input would be compressed into the buffer */
          ws_prepare (WS_MAX);
          e = _swix (RC_SQ_COMPRESS, _INR (0, 5), 0, g_ws, x, 100, cb, 0xF0000000u);
          emit ("squash.edge compress-bad-output-length error=%s", etext (e));
          ws_prepare (WS_MAX);
          e = _swix (RC_SQ_DECOMPRESS, _INR (0, 5), 0, g_ws, cb, 0, dz, 0xF0000000u);
          emit ("squash.edge decompress-bad-output-length error=%s", etext (e));
          e = _swix (RC_SQ_DECOMPRESS + 2, 0);
          emit ("squash.edge swi-2 error=%s", etext (e));
        }
      }
    else fail ("no memory for the edge cases");
    free (x); free (cb); free (dz);
  }
  free (g_ws);
  fflush (out);
  printf ("squash: %u problems\n", n_fail - before);
}

/* ------------------------------------------------------------------------------------------------ MimeMap */
static unsigned mm_ok;                                                   /* how many conversions of the last mm_describe worked */

static const _kernel_oserror *mm (unsigned from, unsigned val, unsigned to, void *buf, unsigned *r3, unsigned *r4)
{
  unsigned o3 = 0, o4 = 0;
  const _kernel_oserror *e = _swix (RC_MIME_TRANSLATE, _INR (0, 3) | _OUTR (3, 4), from, val, to, buf, &o3, &o4);
  if (!e) { *r3 = o3; *r4 = o4; }
  return e;
}

static char *mm_cat (char *d, const char *end, const char *fmt, ...)
{
  va_list ap;
  int n;
  if (d >= end) return d;
  va_start (ap, fmt);
  n = vsnprintf (d, (size_t) (end - d), fmt, ap);
  va_end (ap);
  return n > 0 ? (d + n < end ? d + n : (char *) end - 1) : d;
}

/* all the answers for one input: MIME type, file type number, file type name, extension, all extensions */
static void mm_describe (char *dst, size_t cap, unsigned from, unsigned val)
{
  char b[400];
  unsigned r3 = 0, r4 = 0;
  const _kernel_oserror *e;
  char *d = dst;
  const char *end = dst + cap;
  mm_ok = 0;
  memset (b, 0, sizeof b);
  e = mm (from, val, 2, b, &r3, &r4);
  if (!e) mm_ok++;
  d = mm_cat (d, end, "mime=%s", e ? enum_ (e) : b);
  e = mm (from, val, 0, 0, &r3, &r4);
  if (!e) mm_ok++;
  d = mm_cat (d, end, " ft=");
  d = e ? mm_cat (d, end, "%s", enum_ (e)) : mm_cat (d, end, "&%X", r3);
  memset (b, 0, sizeof b);
  e = mm (from, val, 1, b, &r3, &r4);
  if (!e) mm_ok++;
  d = mm_cat (d, end, " name=%s", e ? enum_ (e) : b);
  memset (b, 0, sizeof b);
  e = mm (from, val, 3, b, &r3, &r4);
  if (!e) mm_ok++;
  d = mm_cat (d, end, " ext=%s", e ? enum_ (e) : b);
  {
    char **list = NULL;
    unsigned cnt = 0, k;
    e = mm (from, val, 5, &list, &r3, &cnt);
    if (!e) mm_ok++;
    d = mm_cat (d, end, " exts=");
    if (e) d = mm_cat (d, end, "%s", enum_ (e));
    else if (cnt > 20 || (!list && cnt)) d = mm_cat (d, end, "count=%u?", cnt);
    else for (k = 0; k < cnt; k++) d = mm_cat (d, end, "%s%s", k ? "," : "", list[k]);
  }
}

static void mm_expect_str (const char *what, unsigned from, unsigned val, unsigned to, const char *want)   /* want NULL: an error is expected */
{
  char b[400];
  unsigned r3 = 0, r4 = 0;
  const _kernel_oserror *e;
  memset (b, 0, sizeof b);
  e = mm (from, val, to, b, &r3, &r4);
  if (want) CHECK (!e && strcmp (b, want) == 0, "%s: '%s' (%s), expected '%s'", what, b, etext (e), want);
  else CHECK (e != NULL, "%s: expected an error, got '%s'", what, b);
}
static void mm_expect_ft (const char *what, unsigned from, unsigned val, unsigned want)
{
  unsigned r3 = 0, r4 = 0;
  const _kernel_oserror *e = mm (from, val, 0, 0, &r3, &r4);
  CHECK (!e && r3 == want, "%s: &%X (%s), expected &%X", what, r3, etext (e), want);
}

/* the lines of a mapping file: the MIME types, the file type names and numbers, the extensions */
typedef struct { char **v; unsigned n, cap; } slist_t;
static void sl_add (slist_t *l, const char *s)
{
  unsigned k;
  for (k = 0; k < l->n; k++) if (strcmp (l->v[k], s) == 0) return;
  if (l->n == l->cap)
    {
      char **nv = realloc (l->v, (l->cap ? l->cap * 2 : 64) * sizeof (char *));
      if (!nv) return;
      l->v = nv; l->cap = l->cap ? l->cap * 2 : 64;
    }
  l->v[l->n] = malloc (strlen (s) + 1);
  if (l->v[l->n]) { strcpy (l->v[l->n++], s); }
}
static void sl_free (slist_t *l) { unsigned k; for (k = 0; k < l->n; k++) free (l->v[k]); free (l->v); l->v = NULL; l->n = l->cap = 0; }

static void mime_dataset (const char *label, const char *path, int is_test)
{
  char line[512], *text, *p, *nl;
  size_t len = 0;
  const _kernel_oserror *e;
  slist_t mimes = { 0, 0, 0 }, names = { 0, 0, 0 }, hexes = { 0, 0, 0 }, exts = { 0, 0, 0 };
  unsigned ft, mapped = 0, unmapped = 0, k;
  uint32_t crc = 0;
  /* the module reads the file that Inet$MimeMappings names when it starts (mime_set has set the variable and started the module again); the program reads the same file for the names to ask for */
  text = slurp (path, &len);
  emit ("mime.%s dataset '%s' bytes=%u", label, strrchr (path, '.') ? strrchr (path, '.') + 1 : path, (unsigned) len);
  if (!text) { fail ("the mapping file %s cannot be read", path); return; }
  for (p = text; *p; p = nl ? nl + 1 : p + strlen (p))
    {
      char *tok[12], *q, *saved;
      unsigned nt = 0;
      nl = strchr (p, '\n');
      if (nl) *nl = 0;
      q = p;
      while (*q == ' ' || *q == '\t' || *q == '\r') q++;
      if (*q && *q != '#')
        {
          saved = q;
          for (;;)
            {
              while (*q == ' ' || *q == '\t' || *q == '\r') q++;
              if (!*q || nt == 12) break;
              tok[nt++] = q;
              while (*q && *q != ' ' && *q != '\t' && *q != '\r') q++;
              if (*q) *q++ = 0;
            }
          (void) saved;
          if (nt >= 3)
            {
              if (!strchr (tok[0], '*')) sl_add (&mimes, tok[0]);
              if (tok[1][0] != '*') sl_add (&names, tok[1]);
              if (tok[2][0] != '*') sl_add (&hexes, tok[2]);
              for (k = 3; k < nt; k++) if (tok[k][0] == '.') sl_add (&exts, tok[k]);
            }
        }
      if (!nl) break;
    }
  emit ("mime.%s parsed mimes=%u names=%u numbers=%u extensions=%u", label, mimes.n, names.n, hexes.n, exts.n);
  /* every file type number (the quick run, for the interpreter: those of the file and four more) */
  {
    unsigned nft = quick ? hexes.n + 4 : 0x1000;
    for (k = 0; k < nft; k++)
      {
        if (!quick) ft = k;
        else if (k < hexes.n) ft = (unsigned) strtoul (hexes.v[k], NULL, 16);
        else ft = k == hexes.n ? 0u : k == hexes.n + 1 ? 1u : k == hexes.n + 2 ? 0x123u : 0xFFFu;
        mm_describe (line, sizeof line, 0, ft);
        crc = crc32 (crc, line, strlen (line));
        if (mm_ok) { emit ("mime.%s.ft &%03X %s", label, ft, line); mapped++; }
        else unmapped++;
      }
  }
  emit ("mime.%s.ft summary mapped=%u unmapped=%u crc=%08x", label, mapped, unmapped, (unsigned) crc);
  if (quick)                                                            /* at most 40 of each, for the interpreter */
    {
      if (mimes.n > 40) mimes.n = 40;
      if (exts.n > 40) exts.n = 40;
      if (names.n > 40) names.n = 40;
      if (hexes.n > 40) hexes.n = 40;
    }
  for (k = 0; k < mimes.n; k++)
    {
      char up[260];
      unsigned j;
      mm_describe (line, sizeof line, 2, (unsigned) (uintptr_t) mimes.v[k]);
      emit ("mime.%s.mime %s %s", label, mimes.v[k], line);
      snprintf (up, sizeof up, "%s", mimes.v[k]);
      for (j = 0; up[j]; j++) if (up[j] >= 'a' && up[j] <= 'z') up[j] -= 32;
      mm_describe (line, sizeof line, 2, (unsigned) (uintptr_t) up);
      emit ("mime.%s.mime-upper %s %s", label, up, line);
    }
  for (k = 0; k < exts.n; k++)
    {
      char up[100];
      unsigned j;
      mm_describe (line, sizeof line, 3, (unsigned) (uintptr_t) exts.v[k]);
      emit ("mime.%s.ext %s %s", label, exts.v[k], line);
      snprintf (up, sizeof up, "%s", exts.v[k]);
      for (j = 0; up[j]; j++) if (up[j] >= 'a' && up[j] <= 'z') up[j] -= 32;
      mm_describe (line, sizeof line, 3, (unsigned) (uintptr_t) up);
      emit ("mime.%s.ext-upper %s %s", label, up, line);
    }
  for (k = 0; k < names.n; k++)
    {
      mm_describe (line, sizeof line, 1, (unsigned) (uintptr_t) names.v[k]);
      emit ("mime.%s.name %s %s", label, names.v[k], line);
    }
  for (k = 0; k < hexes.n; k++)
    {
      char s[24];
      snprintf (s, sizeof s, "&%s", hexes.v[k]);
      mm_describe (line, sizeof line, 1, (unsigned) (uintptr_t) s);
      emit ("mime.%s.hex %s %s", label, s, line);
    }
  /* things that are not in the file */
  {
    static const char *const odd_mime[] = { "foo/bar", "text/", "/plain", "text", "", "text/plain extra words", "TEXT/PLAIN", "image/x-nothing" };
    static const char *const odd_ext[] = { ".nosuchext", "nosuchext", ".", "", ".txt.txt", "txt" };
    unsigned j;
    for (j = 0; j < sizeof odd_mime / sizeof odd_mime[0]; j++)
      {
        mm_describe (line, sizeof line, 2, (unsigned) (uintptr_t) odd_mime[j]);
        emit ("mime.%s.odd-mime '%s' %s", label, odd_mime[j], line);
      }
    for (j = 0; j < sizeof odd_ext / sizeof odd_ext[0]; j++)
      {
        mm_describe (line, sizeof line, 3, (unsigned) (uintptr_t) odd_ext[j]);
        emit ("mime.%s.odd-ext '%s' %s", label, odd_ext[j], line);
      }
  }
  /* the errors of the SWI */
  {
    unsigned r3 = 0, r4 = 0, from;
    char b[64];
    for (from = 4; from <= 7; from++)
      {
        e = mm (from, (unsigned) (uintptr_t) "text/plain", 2, b, &r3, &r4);
        emit ("mime.%s.reason-%u error=%s", label, from, etext (e));
        CHECK (e != NULL && e->errnum == 0xB00001, "MimeMap_Translate with R0 = %u: %s (expected &B00001)", from, etext (e));
      }
    e = mm (99, 0, 2, b, &r3, &r4);
    CHECK (e != NULL && e->errnum == 0xB00001, "MimeMap_Translate with R0 = 99: %s", etext (e));
    e = mm (0, 0xABC, 2, b, &r3, &r4);
    emit ("mime.%s.output-9 error=%s", label, etext (mm (2, (unsigned) (uintptr_t) "text/plain", 9, b, &r3, &r4)));
    e = _swix (RC_MIME_TRANSLATE + 1, 0);
    emit ("mime.%s.swi-1 error=%s", label, etext (e));
  }
  /* what is known about the content of the test file */
  if (is_test)
    {
      char **list = NULL;
      unsigned r3 = 0, cnt = 0;
      mm_expect_str ("&FFF to MIME", 0, 0xFFF, 2, "text/plain");
      mm_expect_str ("&FAF to MIME", 0, 0xFAF, 2, "text/html");
      mm_expect_str ("&C85 to MIME", 0, 0xC85, 2, "image/jpeg");
      mm_expect_str ("&ABC to MIME", 0, 0xABC, 2, "application/x-romcmp-test");
      mm_expect_str ("&123 to MIME (the entry whose file type is * has it)", 0, 0x123, 2, "application/riscos");
      mm_expect_str ("&FFF to the first extension", 0, 0xFFF, 3, "txt");
      mm_expect_str ("&C85 to the first extension", 0, 0xC85, 3, "jpg");
      mm_expect_str ("&FFF to its name", 0, 0xFFF, 1, "Text");
      mm_expect_ft ("text/plain to a file type", 2, (unsigned) (uintptr_t) "text/plain", 0xFFF);
      mm_expect_ft ("TEXT/Plain to a file type (upper case)", 2, (unsigned) (uintptr_t) "TEXT/Plain", 0xFFF);
      mm_expect_ft ("text/unheard-of to a file type (text/*)", 2, (unsigned) (uintptr_t) "text/unheard-of", 0xFFF);
      mm_expect_ft ("image/jpeg to a file type", 2, (unsigned) (uintptr_t) "image/jpeg", 0xC85);
      mm_expect_ft (".txt to a file type", 3, (unsigned) (uintptr_t) ".txt", 0xFFF);
      mm_expect_ft ("txt to a file type (no dot)", 3, (unsigned) (uintptr_t) "txt", 0xFFF);
      mm_expect_ft (".JPEG to a file type", 3, (unsigned) (uintptr_t) ".JPEG", 0xC85);
      mm_expect_ft (".wibble to a file type", 3, (unsigned) (uintptr_t) ".wibble", 0xABC);
      mm_expect_ft ("Text to a file type", 1, (unsigned) (uintptr_t) "Text", 0xFFF);
      mm_expect_str ("text/html to the first extension", 2, (unsigned) (uintptr_t) "text/html", 3, "html");
      mm_expect_str (".htm to MIME", 3, (unsigned) (uintptr_t) ".htm", 2, "text/html");
      e = mm (0, 0xC85, 5, &list, &r3, &cnt);
      CHECK (!e && cnt == 3 && list && strcmp (list[0], "jpg") == 0 && strcmp (list[1], "jpeg") == 0 && strcmp (list[2], "jpe") == 0, "the extensions of &C85: %u, first '%s' (%s)", cnt, list ? list[0] : "-", etext (e));
      e = mm (0, 0xFFF, 5, &list, &r3, &cnt);
      CHECK (!e && cnt == 2 && list && strcmp (list[0], "txt") == 0 && strcmp (list[1], "text") == 0, "the extensions of &FFF: %u (%s)", cnt, etext (e));
      e = mm (0, 0x123, 0, 0, &r3, &cnt);
      CHECK (!e && r3 == 0x123, "&123 to a file type: &%X (%s)", r3, etext (e));
    }
  /* the commands */
  emit_capture ("mimemap-all", "MimeMap", 0);
  emit_capture ("mimemap-ext", "MimeMap .txt", 0);
  emit_capture ("mimemap-hex", "MimeMap &FFF", 0);
  emit_capture ("mimemap-mime", "MimeMap text/plain", 0);
  emit_capture ("mimemap-name", "MimeMap Text", 0);
  emit_capture ("mimemap-bad", "MimeMap nosuch/type-here", 0);
  emit_capture ("mimemap-badext", "MimeMap .nosuchext", 0);
  emit_capture ("readmimemap", "ReadMimeMap", 0);
  emit_capture ("help", "Help MimeMap", 1);
  sl_free (&mimes); sl_free (&names); sl_free (&hexes); sl_free (&exts);
  free (text);
  fflush (out);
}

/* points Inet$MimeMappings at a mapping file and starts the module again so that it reads it, then runs the questions */
static void mime_set (const char *label, const char *path, int is_test)
{
  char c[400];
  const _kernel_oserror *e;
  snprintf (c, sizeof c, "Set Inet$MimeMappings %s", path);
  e = _swix (OS_CLI, _IN (0), c);
  emit ("mime.%s set-variable error=%s", label, enum_ (e));
  CHECK (!e, "mime.%s: %s: %s", label, c, etext (e));
  e = _swix (OS_CLI, _IN (0), "RMReInit MimeMap");
  emit ("mime.%s reinit error=%s", label, enum_ (e));
  CHECK (!e, "mime.%s: RMReInit MimeMap: %s", label, etext (e));
  mime_dataset (label, path, is_test);
}

/* ------------------------------------------------------------------------------------------------ DrawFile */
typedef struct { uint8_t *p; unsigned len, cap; } dbuf_t;
static void w32 (dbuf_t *d, uint32_t v) { if (d->len + 4 <= d->cap) memcpy (d->p + d->len, &v, 4); d->len += 4; }
static void wbytes (dbuf_t *d, const void *s, unsigned n)                /* n bytes, padded with zeros to a word */
{
  unsigned padded = (n + 3) & ~3u;
  if (d->len + padded <= d->cap) { memset (d->p + d->len, 0, padded); memcpy (d->p + d->len, s, n); }
  d->len += padded;
}
#define U(v) ((int) (v) * 256)                                           /* OS units to Draw units */

static void d_header (dbuf_t *d, int x0, int y0, int x1, int y1)
{
  d->len = 0;
  wbytes (d, "Draw", 4); w32 (d, 201); w32 (d, 0); wbytes (d, "RomCmp      ", 12);
  w32 (d, (uint32_t) x0); w32 (d, (uint32_t) y0); w32 (d, (uint32_t) x1); w32 (d, (uint32_t) y1);
}
static unsigned o_begin (dbuf_t *d, int type, int x0, int y0, int x1, int y1)
{
  unsigned at = d->len;
  w32 (d, (uint32_t) type); w32 (d, 0); w32 (d, (uint32_t) x0); w32 (d, (uint32_t) y0); w32 (d, (uint32_t) x1); w32 (d, (uint32_t) y1);
  return at;
}
static void o_end (dbuf_t *d, unsigned at) { uint32_t sz = d->len - at; memcpy (d->p + at + 4, &sz, 4); }

static void o_path (dbuf_t *d, int bx0, int by0, int bx1, int by1, uint32_t fill, uint32_t outline, int width, unsigned style, const int *dash, const int *elems, unsigned nelem)
{
  unsigned at = o_begin (d, 2, bx0, by0, bx1, by1), i;
  w32 (d, fill); w32 (d, outline); w32 (d, (uint32_t) width); w32 (d, style);
  if (dash) { w32 (d, (uint32_t) dash[0]); w32 (d, (uint32_t) dash[1]); for (i = 0; i < (unsigned) dash[1]; i++) w32 (d, (uint32_t) dash[2 + i]); }
  for (i = 0; i < nelem; i++) w32 (d, (uint32_t) elems[i]);
  o_end (d, at);
}

static const uint32_t RED = 0x0000FF00u, GREEN = 0x00FF0000u, BLUE = 0xFF000000u, YELLOW = 0x00FFFF00u, BLACK = 0, CLEAR = 0xFFFFFFFFu;
#define RECT(x0, y0, x1, y1) 2, U (x0), U (y0), 8, U (x1), U (y0), 8, U (x1), U (y1), 8, U (x0), U (y1), 5, 0

static void d_rect (dbuf_t *d)
{
  static const int e[] = { RECT (0, 0, 80, 80) };
  d_header (d, 0, 0, U (80), U (80));
  o_path (d, 0, 0, U (80), U (80), RED, CLEAR, 0, 0, NULL, e, sizeof e / sizeof e[0]);
}
static void d_stroke (dbuf_t *d)
{
  static const int e[] = { 2, U (10), U (10), 8, U (90), U (20), 8, U (30), U (90), 0 };
  d_header (d, 0, 0, U (100), U (100));
  o_path (d, 0, 0, U (100), U (100), CLEAR, BLUE, U (8), 0x15, NULL, e, sizeof e / sizeof e[0]);
}
static void d_dash (dbuf_t *d)
{
  static const int e[] = { 2, U (5), U (50), 8, U (95), U (50), 0 };
  static const int dash[] = { 0, 2, U (10), U (5) };
  d_header (d, 0, 0, U (100), U (100));
  o_path (d, 0, 0, U (100), U (100), CLEAR, GREEN, U (4), 0x80, dash, e, sizeof e / sizeof e[0]);
}
static void d_bezier (dbuf_t *d)
{
  static const int e[] = { 2, U (10), U (10), 6, U (40), U (90), U (60), U (90), U (90), U (10), 5, 0 };
  d_header (d, 0, 0, U (100), U (100));
  o_path (d, 0, 0, U (100), U (100), YELLOW, BLACK, U (2), 0, NULL, e, sizeof e / sizeof e[0]);
}
static void d_group (dbuf_t *d)
{
  static const int r1[] = { RECT (0, 0, 60, 60) }, r2[] = { RECT (30, 30, 90, 90) }, donut[] = { RECT (10, 10, 100, 100), RECT (30, 30, 80, 80) };
  unsigned g;
  d_header (d, 0, 0, U (100), U (100));
  g = o_begin (d, 6, 0, 0, U (100), U (100));
  wbytes (d, "grp", 4); w32 (d, 0); w32 (d, 0);
  o_path (d, 0, 0, U (60), U (60), RED, CLEAR, 0, 0, NULL, r1, sizeof r1 / sizeof r1[0]);
  o_path (d, U (30), U (30), U (90), U (90), BLUE, CLEAR, 0, 0, NULL, r2, sizeof r2 / sizeof r2[0]);
  o_path (d, U (10), U (10), U (100), U (100), GREEN, CLEAR, 0, 0x40, NULL, donut, sizeof donut / sizeof donut[0]);
  o_end (d, g);
}
static void d_tagged (dbuf_t *d)
{
  static const int e[] = { RECT (5, 5, 70, 50) };
  unsigned t;
  d_header (d, 0, 0, U (80), U (60));
  t = o_begin (d, 7, 0, 0, U (80), U (60));
  w32 (d, 0x12345678);
  o_path (d, U (5), U (5), U (70), U (50), BLUE, BLACK, U (1), 0, NULL, e, sizeof e / sizeof e[0]);
  o_end (d, t);
}
static void d_text (dbuf_t *d)                                           /* text in the system font (font number 0) */
{
  unsigned t;
  d_header (d, 0, 0, U (64), U (40));
  t = o_begin (d, 1, 0, 0, U (64), U (40));
  w32 (d, GREEN); w32 (d, CLEAR); w32 (d, 0); w32 (d, (uint32_t) U (16)); w32 (d, (uint32_t) U (32)); w32 (d, (uint32_t) U (4)); w32 (d, (uint32_t) U (8));
  wbytes (d, "Rom37", 6);
  o_end (d, t);
}
static void d_sprite (dbuf_t *d)                                         /* a 4 x 4 pixel sprite of 32 bits per pixel in a Draw file */
{
  unsigned s, i;
  d_header (d, 0, 0, U (64), U (64));
  s = o_begin (d, 5, 0, 0, U (64), U (64));
  w32 (d, 0x2C + 64); wbytes (d, "spr\0\0\0\0\0\0\0\0\0", 12); w32 (d, 3); w32 (d, 3); w32 (d, 0); w32 (d, 31); w32 (d, 0x2C); w32 (d, 0x2C); w32 (d, 0x301680B5u);
  for (i = 0; i < 16; i++) w32 (d, 0x00000000u | ((i * 16) << 16) | (((15 - i) * 16) << 8) | 0x80);
  o_end (d, s);
}

static const struct { const char *name; void (*make) (dbuf_t *); } d_cases[] = {
  { "rect", d_rect }, { "stroke", d_stroke }, { "dash", d_dash }, { "bezier", d_bezier }, { "group", d_group }, { "tagged", d_tagged }, { "text", d_text }, { "sprite", d_sprite } };

static const int d_matrices[][6] = {
  { 0x10000, 0, 0, 0x10000, 0, 0 }, { 0x20000, 0, 0, 0x20000, 0, 0 }, { 0x10000, 0, 0, 0x10000, 1000, -2000 }, { 0, 0x10000, -0x10000, 0, 0, 0 },
  { 46341, 46341, -46341, 46341, 0, 0 }, { -0x10000, 0, 0, 0x10000, 5000, 0 }, { 0x18000, 0, 0x4000, 0x8000, -300, 700 } };

static void df_bbox (const dbuf_t *d, int size, const int *m, int *box, const _kernel_oserror **e)
{
  *e = _swix (RC_DF_BBOX, _INR (0, 4), 0, d->p, size, m, box);
}

static void draw_suite (void)
{
  dbuf_t d;
  unsigned c, m, before = n_fail, b;
  d.cap = 4096; d.len = 0; d.p = malloc (d.cap + 16);
  if (!d.p) { fail ("no memory for the Draw files"); return; }
  for (c = 0; c < sizeof d_cases / sizeof d_cases[0]; c++)
    {
      const _kernel_oserror *e;
      int box[4], hdr[4];
      d_cases[c].make (&d);
      memcpy (hdr, d.p + 24, 16);
      emit ("draw.%s file bytes=%u crc=%08x", d_cases[c].name, d.len, (unsigned) crc32 (0, d.p, d.len));
      for (m = 0; m < sizeof d_matrices / sizeof d_matrices[0]; m++)
        {
          box[0] = box[1] = box[2] = box[3] = 0x7EADBEEF;
          df_bbox (&d, (int) d.len, m == 0 ? NULL : d_matrices[m], box, &e);
          emit ("draw.%s bbox.%u (%d,%d,%d,%d) error=%s", d_cases[c].name, m, box[0], box[1], box[2], box[3], etext (e));
          if (m == 0) CHECK (!e && box[0] == hdr[0] && box[1] == hdr[1] && box[2] == hdr[2] && box[3] == hdr[3], "%s: the box with the identity is (%d,%d,%d,%d), not the box of the file (%d,%d,%d,%d) (%s)", d_cases[c].name, box[0], box[1], box[2], box[3], hdr[0], hdr[1], hdr[2], hdr[3], etext (e));
          if (m == 1) CHECK (!e && box[0] == 2 * hdr[0] && box[1] == 2 * hdr[1] && box[2] == 2 * hdr[2] && box[3] == 2 * hdr[3], "%s: the box at twice the size is (%d,%d,%d,%d)", d_cases[c].name, box[0], box[1], box[2], box[3]);
          if (m == 2) CHECK (!e && box[0] == hdr[0] + 1000 && box[1] == hdr[1] - 2000 && box[2] == hdr[2] + 1000 && box[3] == hdr[3] - 2000, "%s: the moved box is (%d,%d,%d,%d)", d_cases[c].name, box[0], box[1], box[2], box[3]);
          if (m == 3) CHECK (!e && box[0] == -hdr[3] && box[1] == hdr[0] && box[2] == -hdr[1] && box[3] == hdr[2], "%s: the box turned by 90 degrees is (%d,%d,%d,%d)", d_cases[c].name, box[0], box[1], box[2], box[3]);
          if (m == 5) CHECK (!e && box[0] == 5000 - hdr[2] && box[1] == hdr[1] && box[2] == 5000 - hdr[0] && box[3] == hdr[3], "%s: the mirrored box is (%d,%d,%d,%d)", d_cases[c].name, box[0], box[1], box[2], box[3]);
        }
      e = _swix (RC_DF_DECLARE, _INR (0, 2), 0, d.p, d.len);
      emit ("draw.%s declarefonts error=%s", d_cases[c].name, etext (e));
      e = _swix (RC_DF_RENDER, _INR (0, 5), 2 /* suppress: walk the objects, paint nothing */, d.p, d.len, 0, hdr, 0);
      emit ("draw.%s render-suppressed error=%s", d_cases[c].name, etext (e));
    }
  /* damaged files: what the verification says */
  {
    static const char *const names[] = { "badtag", "tiny", "version202", "objectover", "groupover", "fonttable2", "nofont", "tagover", "trfmnofont", "unknowntype", "emptydiagram", "version201" };
    for (b = 0; b < sizeof names / sizeof names[0]; b++)
      {
        const _kernel_oserror *e;
        int box[4] = { 0, 0, 0, 0 }, size;
        unsigned o;
        static const int e2[] = { RECT (0, 0, 10, 10) };
        d_rect (&d);
        size = (int) d.len;
        switch (b)
          {
          case 0: memcpy (d.p, "Drow", 4); break;
          case 1: size = 20; break;
          case 2: { uint32_t v = 202; memcpy (d.p + 4, &v, 4); } break;
          case 3: { uint32_t v = 0x1000; memcpy (d.p + 40 + 4, &v, 4); } break;                          /* the size of the first object */
          case 4:                                                                                           /* a group whose member is longer than the group */
            d_header (&d, 0, 0, U (10), U (10));
            o = o_begin (&d, 6, 0, 0, U (10), U (10)); wbytes (&d, "g\0\0\0\0\0\0\0\0\0\0\0", 12);
            { unsigned in = d.len; uint32_t v = 0x500; o_path (&d, 0, 0, U (10), U (10), RED, CLEAR, 0, 0, NULL, e2, sizeof e2 / sizeof e2[0]); memcpy (d.p + in + 4, &v, 4); }
            o_end (&d, o); size = (int) d.len; break;
          case 5:                                                                                           /* two font tables */
            d_header (&d, 0, 0, U (10), U (10));
            for (c = 0; c < 2; c++) { static const uint8_t fd[] = { 1, 'A', 'B', 'C', 0 }; o = o_begin (&d, 0, 0, 0, 0, 0); wbytes (&d, fd, 5); o_end (&d, o); }
            size = (int) d.len; break;
          case 6:                                                                                           /* text in font 5 and no font table */
            d_text (&d);
            { uint32_t v = 5; memcpy (d.p + 40 + 24 + 8, &v, 4); }
            size = (int) d.len; break;
          case 7:                                                                                           /* a tagged object whose member is longer than it */
            d_header (&d, 0, 0, U (10), U (10));
            o = o_begin (&d, 7, 0, 0, U (10), U (10)); w32 (&d, 1);
            { unsigned in = d.len; uint32_t v = 0x500; o_path (&d, 0, 0, U (10), U (10), RED, CLEAR, 0, 0, NULL, e2, sizeof e2 / sizeof e2[0]); memcpy (d.p + in + 4, &v, 4); }
            o_end (&d, o); size = (int) d.len; break;
          case 8:                                                                                           /* transformed text in font 3 and no font table */
            d_header (&d, 0, 0, U (10), U (10));
            o = o_begin (&d, 12, 0, 0, U (10), U (10));
            for (c = 0; c < 6; c++) w32 (&d, (c == 0 || c == 3) ? 0x10000u : 0u);
            w32 (&d, 0); w32 (&d, GREEN); w32 (&d, CLEAR); w32 (&d, 3); w32 (&d, 0x100); w32 (&d, 0x100); w32 (&d, 0); w32 (&d, 0); wbytes (&d, "x", 2);
            o_end (&d, o); size = (int) d.len; break;
          case 9:                                                                                           /* an object type that nothing knows */
            d_header (&d, 0, 0, U (10), U (10));
            o = o_begin (&d, 99, 0, 0, 0, 0); w32 (&d, 0); o_end (&d, o); size = (int) d.len; break;
          case 10: d_header (&d, 1, 2, 3, 4); size = (int) d.len; break;
          case 11: break;
          }
        e = _swix (RC_DF_BBOX, _INR (0, 4), 0, d.p, size, 0, box);
        emit ("draw.verify.%s bbox error=%s box=(%d,%d,%d,%d)", names[b], etext (e), box[0], box[1], box[2], box[3]);
        if (b == 0 || b == 1 || b == 2 || b == 3 || b == 4 || b == 5 || b == 6 || b == 7 || b == 8) CHECK (e != NULL, "draw.verify.%s: expected an error", names[b]);
        else CHECK (!e, "draw.verify.%s: unexpected error %s", names[b], etext (e));
      }
  }
  /* an SWI of the chunk that does not exist */
  {
    const _kernel_oserror *e;
    d_rect (&d);
    e = _swix (RC_DF_RENDER + 3, _INR (1, 2), d.p, d.len);
    emit ("draw.swi-3 error=%s", etext (e));
  }
  emit_capture ("help-drawfile", "Help DrawFile", 1);
  emit_capture ("help-render", "Help Render", 0);
  emit_capture ("render-syntax", "Render", 0);
  free (d.p);
  fflush (out);
  printf ("draw: %u problems\n", n_fail - before);
}

/* ------------------------------------------------------------------------------------------------ DrawFile_Render into a sprite of 32 bits per pixel (needs the Draw, ColourTrans and Font modules and the sprite code of the OS) */
#define SPR_W 128
#define SPR_H 128
#define SPR_MODE 0x301680B5u                                              /* 32 bits per pixel, 90 dpi */
#define SPR_SIZE (16 + 44 + SPR_W * SPR_H * 4 + 64)
#define BG 0x00FFFFFFu                                                   /* the background of the sprite: white */

typedef struct { const char *name; void (*make) (dbuf_t *); unsigned flags; const int *matrix; } rcase_t;
static void render_suite (void)
{
  static const int m_scale[] = { 0x18000, 0, 0, 0x18000, 20 * 256, 10 * 256 };
  static const int m_turn[] = { 0, 0x10000, -0x10000, 0, 100 * 256, 0 };
  static const rcase_t cases[] = {
    { "rect", d_rect, 0, NULL }, { "stroke", d_stroke, 0, NULL }, { "dash", d_dash, 0, NULL }, { "bezier", d_bezier, 0, NULL }, { "group", d_group, 0, NULL }, { "tagged", d_tagged, 0, NULL },
    { "rect-scaled", d_rect, 0, m_scale }, { "stroke-turned", d_stroke, 0, m_turn }, { "rect-bboxes", d_rect, 1, NULL }, { "rect-suppressed", d_rect, 2, NULL },
    { "text", d_text, 0, NULL }, { "sprite", d_sprite, 0, NULL }, { "group-scaled", d_group, 0, m_scale } };
  unsigned c, before = n_fail;
  dbuf_t d;
  uint32_t *area = malloc (SPR_SIZE);
  uint8_t *save = NULL;
  d.cap = 4096; d.len = 0; d.p = malloc (d.cap + 16);
  if (!area || !d.p) { fail ("no memory for the sprite"); return; }
  for (c = 0; c < sizeof cases / sizeof cases[0]; c++)
    {
      const _kernel_oserror *e, *er = NULL;
      char ertext[220] = "none";
      unsigned c0 = 0, c1 = 0, c2 = 0, c3 = 0, ssize = 0, k, nz = 0;
      uint32_t *img, crc;
      int clip[4] = { 0, 0, SPR_W * 2, SPR_H * 2 };
      int minx = 999, miny = 999, maxx = -1, maxy = -1, x, y;
      area[0] = SPR_SIZE; area[1] = 0; area[2] = 16; area[3] = 16;
      e = _swix (OS_SpriteOp, _INR (0, 1), 256 + 9, area);
      if (!e) e = _swix (OS_SpriteOp, _INR (0, 6), 256 + 15, area, "rc", 0, SPR_W, SPR_H, SPR_MODE);
      if (!e) e = _swix (OS_SpriteOp, _INR (0, 2) | _OUT (3), 256 + 62, area, "rc", &ssize);
      if (e) { emit ("render.%s sprite error=%s", cases[c].name, etext (e)); fail ("render.%s: no sprite: %s", cases[c].name, etext (e)); continue; }
      free (save);
      save = malloc (ssize + 16);
      if (!save) { fail ("no memory for the save area"); break; }
      memset (save, 0, ssize + 16);
      img = (uint32_t *) ((uint8_t *) area + 16 + area[16 / 4 + 8]);
      for (k = 0; k < SPR_W * SPR_H; k++) img[k] = BG;
      cases[c].make (&d);
      /* output to the sprite: nothing may be printed until it is back */
      e = _swix (OS_SpriteOp, _INR (0, 3) | _OUTR (0, 3), 256 + 60, area, "rc", save, &c0, &c1, &c2, &c3);
      if (!e)
        {
          er = _swix (RC_DF_RENDER, _INR (0, 5), cases[c].flags, d.p, d.len, cases[c].matrix, clip, 0);
          snprintf (ertext, sizeof ertext, "%s", etext (er));                          /* (the error block may be a buffer that the next SWI reuses) */
          e = _swix (OS_SpriteOp, _INR (0, 3), c0, c1, c2, c3);
        }
      if (e) { printf ("render.%s: could not switch the output to the sprite or back: %s\n", cases[c].name, etext (e)); fail ("render.%s: output switch: %s", cases[c].name, etext (e)); continue; }
      for (k = 0; k < SPR_W * SPR_H; k++)
        if (img[k] != BG)
          {
            nz++;
            x = (int) (k % SPR_W); y = (int) (SPR_H - 1 - k / SPR_W);
            if (x < minx) minx = x;
            if (x > maxx) maxx = x;
            if (y < miny) miny = y;
            if (y > maxy) maxy = y;
          }
      crc = crc32 (0, img, SPR_W * SPR_H * 4);
      emit ("render.%s error=%s nonzero=%u box=(%d,%d,%d,%d) crc=%08x", cases[c].name, ertext, nz, nz ? minx : -1, nz ? miny : -1, nz ? maxx : -1, nz ? maxy : -1, (unsigned) crc);
      CHECK (!er, "render.%s: %s", cases[c].name, ertext);
      if (strcmp (cases[c].name, "rect") == 0)
        {
          uint32_t centre = img[(SPR_H - 1 - 20) * SPR_W + 20], outside = img[(SPR_H - 1 - 100) * SPR_W + 100];
          emit ("render.rect centre=%08x outside=%08x", (unsigned) centre, (unsigned) outside);
          CHECK ((centre & 0xFFFFFF) == 0x0000FF && outside == BG, "render.rect: the pixel in the middle of the red square is %08x and one far outside is %08x", (unsigned) centre, (unsigned) outside);
          CHECK (nz > 1400 && nz < 1800 && minx == 0 && miny == 0, "render.rect: %u pixels in (%d,%d)-(%d,%d); 40 x 40 expected at the origin", nz, minx, miny, maxx, maxy);
        }
      if (strcmp (cases[c].name, "rect-suppressed") == 0) CHECK (nz == 0, "render.rect-suppressed painted %u pixels", nz);
      fflush (out);
    }
  free (save); free (area); free (d.p);
  printf ("render: %u problems\n", n_fail - before);
}

/* ------------------------------------------------------------------------------------------------ the commands */
static int cmd_run (int argc, char **argv)
{
  char path[300], tag[80], mapt[300] = "", maps[300] = "";
  int i, do_squash = 0, do_mime = 0, do_draw = 0, do_render = 0;
  gstrans (argv[3], path, sizeof path);
  snprintf (tag, sizeof tag, "%s", argv[2]);
  for (i = 4; i < argc; i++)
    {
      if (!strcmp (argv[i], "quick")) quick = 1;
      else if (!strcmp (argv[i], "squash")) do_squash = 1;
      else if (!strcmp (argv[i], "draw")) do_draw = 1;
      else if (!strcmp (argv[i], "render")) do_render = 1;
      else if (!strcmp (argv[i], "mime") && i + 2 < argc)
        {
          do_mime = 1;
          gstrans (argv[i + 1], mapt, sizeof mapt); gstrans (argv[i + 2], maps, sizeof maps);
          i += 2;
        }
      else { printf ("RomCmp: unknown suite '%s' (mime needs the two mapping files)\n", argv[i]); return 2; }
    }
  if (!do_squash && !do_mime && !do_draw && !do_render) { printf ("RomCmp: no suite named\n"); return 2; }
  out = fopen (path, "w");
  if (!out) { printf ("RomCmp: cannot write %s\n", path); return 2; }
  emit ("# RomCmp run %s%s", tag, quick ? " (quick)" : "");
  info (out);
  swi_names ();
  if (do_squash) squash_suite ();
  if (do_mime)
    {
      unsigned before = n_fail;
      mime_set ("testmap", mapt, 1);
      mime_set ("sysmap", maps, 0);
      printf ("mime: %u problems\n", n_fail - before);
    }
  if (do_draw) draw_suite ();
  if (do_render) render_suite ();
  emit ("# checks=%u failed=%u", n_checks, n_fail);
  fclose (out);
  printf ("RomCmp %s: %u checks, %u failed; results in %s\n", tag, n_checks, n_fail, path);
  return n_fail ? 1 : 0;
}

static char **split_lines (char *t, unsigned *n)                          /* the lines that are compared: not the empty ones, not those that start with # */
{
  unsigned cap = 256;
  char **v = malloc (cap * sizeof (char *)), *p, *nl;
  *n = 0;
  if (!v) return NULL;
  for (p = t; *p; p = nl ? nl + 1 : p + strlen (p))
    {
      size_t l;
      nl = strchr (p, '\n');
      if (nl) *nl = 0;
      l = strlen (p);
      while (l && (p[l - 1] == '\r' || p[l - 1] == ' ')) p[--l] = 0;
      if (l && p[0] != '#')
        {
          if (*n == cap) { char **nv = realloc (v, cap * 2 * sizeof (char *)); if (!nv) return v; v = nv; cap *= 2; }
          v[(*n)++] = p;
        }
      if (!nl) break;
    }
  return v;
}

static int cmd_diff (const char *a, const char *b)
{
  char pa[300], pb[300], *ta, *tb, **la, **lb;
  unsigned na = 0, nb = 0, i = 0, j = 0, same = 0, differ = 0;
  gstrans (a, pa, sizeof pa); gstrans (b, pb, sizeof pb);
  ta = slurp (pa, NULL); tb = slurp (pb, NULL);
  if (!ta || !tb) { printf ("RomCmp diff: cannot read %s\n", !ta ? pa : pb); return 2; }
  la = split_lines (ta, &na); lb = split_lines (tb, &nb);
  if (!la || !lb) { printf ("RomCmp diff: no memory\n"); return 2; }
  while (i < na || j < nb)
    {
      if (i < na && j < nb && strcmp (la[i], lb[j]) == 0) { same++; i++; j++; continue; }
      differ++;
      if (differ <= 60)
        {
          if (i < na) printf ("A %.170s\n", la[i]);
          if (j < nb) printf ("B %.170s\n", lb[j]);
        }
      if (i < na) i++;
      if (j < nb) j++;
    }
  printf ("RomCmp diff: %u lines the same, %u differ (%u lines in A, %u in B)%s\n", same, differ, na, nb, differ > 60 ? "; the first 60 are shown" : "");
  free (la); free (lb); free (ta); free (tb);
  return differ ? 1 : 0;
}

static int cmd_summary (const char *file)                                 /* the verdict of a run, to be printed again when the Obey file has its spool back (the run closes it) */
{
  char path[300], *t, *p, *nl, *line_checks = NULL;
  unsigned nfail = 0, lines = 0;
  gstrans (file, path, sizeof path);
  t = slurp (path, NULL);
  if (!t) { printf ("RomCmp summary: cannot read %s\n", path); return 2; }
  for (p = t; *p; p = nl ? nl + 1 : p + strlen (p))
    {
      nl = strchr (p, '\n');
      if (nl) *nl = 0;
      lines++;
      if (strncmp (p, "FAIL", 4) == 0) { if (++nfail <= 30) printf ("%.170s\n", p); }
      else if (strncmp (p, "# checks=", 9) == 0) line_checks = p;
      if (!nl) break;
    }
  printf ("RomCmp summary of %s: %u lines, %u FAIL lines, %s\n", path, lines, nfail, line_checks ? line_checks : "NO '# checks=' LINE: the run did not finish");
  free (t);
  return nfail || !line_checks ? 1 : 0;
}

static unsigned word_at (const unsigned char *d, size_t off) { return d[off] | d[off + 1] << 8 | d[off + 2] << 16 | (unsigned) d[off + 3] << 24; }

static _kernel_oserror need_error = { 0x1A0001, "RomCmp: the module image failed the check (see above)" };

static int cmd_need (const char *file, const char *title)                  /* the checks of modcheck, for a module that may have no commands */
{
  char path[300];
  size_t len = 0;
  unsigned char *d;
  unsigned init, fin, ti, help, flags, tab, count, i;
  int ok = 1;
  gstrans (file, path, sizeof path);
  d = (unsigned char *) slurp (path, &len);
  if (!d) { printf ("RomCmp need: cannot read %s\n", path); _swix (OS_GenerateError, _IN (0), &need_error); return 2; }
  if (len < 56 || len % 4 || memcmp (d, "\177ELF", 4) == 0) { printf ("RomCmp need: %s is %u bytes: not a module image\n", path, (unsigned) len); ok = 0; }
  if (ok)
    {
      init = word_at (d, 4); fin = word_at (d, 8); ti = word_at (d, 16); help = word_at (d, 20); flags = word_at (d, 48);
      if (init >= len || (fin && fin >= len) || ti >= len || help >= len || flags + 16 > len || flags % 4) { printf ("RomCmp need: %s: the header words do not point into the file\n", path); ok = 0; }
      if (ok && strcmp ((char *) d + ti, title) != 0) { printf ("RomCmp need: %s: the title is '%s', not '%s'\n", path, d + ti, title); ok = 0; }
      if (ok && strncmp ((char *) d + help, title, strlen (title)) != 0) { printf ("RomCmp need: %s: the help string is '%s'\n", path, d + help); ok = 0; }
      if (ok && (word_at (d, flags) != 1 || word_at (d, flags + 4) != 0)) { printf ("RomCmp need: %s: the flags word is %u and the linked address %u\n", path, word_at (d, flags), word_at (d, flags + 4)); ok = 0; }
      if (ok)
        {
          tab = word_at (d, flags + 8); count = word_at (d, flags + 12);
          if (tab % 4 || tab > len || (size_t) tab + 4 * (size_t) count != len || count == 0) { printf ("RomCmp need: %s: the relocation table does not end the file\n", path); ok = 0; }
          for (i = 0; ok && i < count; i++)
            {
              unsigned off = word_at (d, tab + 4 * i);
              if (off % 4 || off + 4 > tab || word_at (d, off) > tab) { printf ("RomCmp need: %s: entry %u of the relocation table (%u) is not right\n", path, i, off); ok = 0; }
            }
          if (ok) printf ("RomCmp need: %s is a module image of %u bytes with %u relocations, title %s, help '%s'\n", path, (unsigned) len, count, title, d + help);
        }
    }
  free (d);
  if (!ok) { _swix (OS_GenerateError, _IN (0), &need_error); return 1; }
  return 0;
}

static int cmd_try (int argc, char **argv)
{
  char cmd[600], t[300];
  int i;
  const _kernel_oserror *e;
  cmd[0] = 0;
  for (i = 2; i < argc; i++)
    {
      gstrans (argv[i], t, sizeof t);
      if (strlen (cmd) + strlen (t) + 2 < sizeof cmd) { if (i > 2) strcat (cmd, " "); strcat (cmd, t); }
    }
  e = _swix (OS_CLI, _IN (0), cmd);
  printf ("try: %s -> %s\n", cmd, e ? etext (e) : "no error");
  return 0;
}

static int cmd_var (const char *how, const char *name)
{
  char path[300], val[600];
  unsigned len = 0;
  const _kernel_oserror *e;
  snprintf (path, sizeof path, "%s.RomCmpVar", scrap ());
  if (strcmp (how, "savevar") == 0)
    {
      FILE *f = fopen (path, "wb");
      if (!f) { printf ("RomCmp: cannot write %s\n", path); return 2; }
      e = _swix (OS_ReadVarVal, _INR (0, 4) | _OUT (2), name, val, (unsigned) sizeof val - 1, 0, 0, &len);
      if (e) { fputs ("-", f); printf ("%s was not set\n", name); }
      else { val[len] = 0; fputs (val, f); printf ("%s is '%s': kept\n", name, val); }
      fclose (f);
      return 0;
    }
  else
    {
      char *t = slurp (path, NULL);
      if (!t) { printf ("RomCmp: nothing kept in %s\n", path); return 2; }
      if (strcmp (t, "-") == 0) { e = _swix (OS_SetVarVal, _INR (0, 4), name, 0, -1, 0, 0); printf ("%s removed (it was not set)\n", name); }
      else { e = _swix (OS_SetVarVal, _INR (0, 4), name, t, (unsigned) strlen (t), 0, 0); printf ("%s set back to '%s'\n", name, t); }
      if (e) printf ("RomCmp: %s\n", etext (e));
      else remove (path);
      free (t);
      return e ? 1 : 0;
    }
}

int main (int argc, char **argv)
{
  if (argc >= 4 && !strcmp (argv[1], "run")) return cmd_run (argc, argv);
  if (argc == 4 && !strcmp (argv[1], "diff")) return cmd_diff (argv[2], argv[3]);
  if (argc == 4 && !strcmp (argv[1], "need")) return cmd_need (argv[2], argv[3]);
  if (argc == 3 && !strcmp (argv[1], "summary")) return cmd_summary (argv[2]);
  if (argc >= 3 && !strcmp (argv[1], "try")) return cmd_try (argc, argv);
  if (argc == 3 && (!strcmp (argv[1], "savevar") || !strcmp (argv[1], "restorevar"))) return cmd_var (argv[1], argv[2]);
  if (argc == 2 && !strcmp (argv[1], "info")) { info (stdout); return 0; }
  puts ("usage: RMRun RomCmp run <tag> <result file> [squash] [mime <TestMap> <SysMap>] [draw] [render] [quick]");
  puts ("       RMRun RomCmp diff <A> <B> | summary <file> | need <module> <title> | try <command> | info | savevar <name> | restorevar <name>");
  return 2;
}
