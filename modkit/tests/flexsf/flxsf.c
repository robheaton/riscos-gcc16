/* flxsf.c - a Wimp task that tests FlexLib and SFLib built for GCC 16 (UnixLib).  It writes <FlexSF$Dir>.Results (one line per check: ok / FAIL / info) and shows one box with the totals.
   Mode from the system variable FlexSF$Mode: "slot" (flex_init (.., 0): the store is in the application's slot) or "da" (flex_init (.., 8 MB): a dynamic area). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <flex.h>
#include <oslib/os.h>
#include <oslib/wimp.h>
#include "sflib/string.h"
#include "sflib/heap.h"
#include "sflib/stack.h"
#include "sflib/msgs.h"
#include "sflib/config.h"
#include "sflib/general.h"

static FILE *out;
static int checks, failed;
static void ok (int cond, const char *what) { checks++; if (!cond) failed++; fprintf (out, "%s %s\n", cond ? "ok  " : "FAIL", what); fflush (out); }
static void info (const char *fmt, ...) { va_list ap; va_start (ap, fmt); fputs ("info ", out); vfprintf (out, fmt, ap); fputc ('\n', out); va_end (ap);  fflush (out); }

static void fill (void *p, int n, int seed) { unsigned char *c = p; for (int i = 0; i < n; i++) c[i] = (unsigned char) (seed + i * 7); }
static int verify (void *p, int n, int seed) { unsigned char *c = p; for (int i = 0; i < n; i++) if (c[i] != (unsigned char) (seed + i * 7)) return 0; return 1; }

static void test_flex (int da)
{
  void *a = NULL, *b = NULL, *c = NULL, *d = NULL;
  char buf[200];
  int i, good, n;

  ok (flex_alloc (&a, 1000), "flex_alloc 1000");
  ok (flex_alloc (&b, 5000), "flex_alloc 5000");
  ok (flex_alloc (&c, 20000), "flex_alloc 20000");
  ok (a && b && c, "three anchors set");
  fill (a, 1000, 1); fill (b, 5000, 2); fill (c, 20000, 3);
  ok (flex_size (&a) >= 1000 && flex_size (&b) >= 5000 && flex_size (&c) >= 20000, "flex_size (>= what was asked: blocks are rounded up)");
  ok (flex_extend (&b, 60000), "flex_extend 5000 -> 60000 (the blocks above it move)");
  ok (verify (a, 1000, 1) && verify (b, 5000, 2) && verify (c, 20000, 3), "the three blocks keep their contents after the extend (anchors updated)");
  fill (b, 60000, 4);
  ok (flex_extend (&b, 2000), "flex_extend 60000 -> 2000 (shrink)");
  ok (verify (b, 2000, 4) && verify (c, 20000, 3), "contents after the shrink");
  ok (flex_midextend (&c, 100, 400), "flex_midextend (400 bytes inserted at 100)");
  ok (verify (c, 100, 3), "midextend: the first 100 bytes unchanged");
  { unsigned char *cc = c; int same = 1; for (i = 100; i < 19900; i++) if (cc[i + 400] != (unsigned char) (3 + i * 7)) { same = 0; break; } ok (same, "midextend: the rest moved up by 400"); }
  ok (flex_midextend (&c, 500, -400), "flex_midextend (at 500, by -400: the 400 bytes inserted at 100 taken out again; bytes beyond 500 move down to 100)");
  ok (verify (c, 20000, 3), "midextend back: the block is as it was");
  ok (flex_reanchor (&d, &c) && d != NULL && c == NULL, "flex_reanchor moves the block to another anchor");
  ok (verify (d, 20000, 3), "the block is intact through its new anchor");
  ok (flex_reanchor (&c, &d) && c != NULL && d == NULL, "flex_reanchor back to the first anchor");
  ok (c != NULL && verify (c, 20000, 3), "block c after reanchor");
  flex_free (&a);
  ok (a == NULL, "flex_free sets the anchor to NULL");
  ok (verify (b, 2000, 4) && verify (c, 20000, 3), "contents after freeing the lowest block");
  good = 1; n = 0;
  { void *blk[40]; memset (blk, 0, sizeof blk);
    for (i = 0; i < 40; i++) { if (!flex_alloc (&blk[i], 10000 + i * 500)) { good = 0; break; } fill (blk[i], 10000 + i * 500, i); n++; }
    for (i = 0; i < n; i += 2) flex_free (&blk[i]);
    for (i = 1; i < n; i += 2) if (!verify (blk[i], 10000 + i * 500, i)) good = 0;
    ok (good && n == 40, "40 blocks of 10 to 30 KB allocated, every other one freed, the others intact");
    for (i = 1; i < n; i += 2) flex_free (&blk[i]); }
  ok (flex_compact () >= 0, "flex_compact");
  { void *big = NULL; int got = flex_alloc (&big, 3 * 1024 * 1024);
    info ("flex_alloc 3 MB gave %d", got);
    if (got) { fill (big, 3 * 1024 * 1024, 9); ok (verify (big, 3 * 1024 * 1024, 9), "3 MB block written and read back"); flex_free (&big); }
    else ok (!da, "3 MB: refused (only a slot without room may refuse; a dynamic area must not)"); }
  /* the C heap and flex side by side: a malloc after flex blocks exist must not damage them */
  { void *m[50]; int allok = 1;
    for (i = 0; i < 50; i++) { m[i] = malloc (20000); if (!m[i]) { allok = 0; break; } memset (m[i], i, 20000); }
    ok (allok, "50 malloc of 20000 bytes (1 MB) while flex blocks exist");
    ok (verify (b, 2000, 4) && verify (c, 20000, 3), "the flex blocks are intact after the mallocs");
    for (i = 0; i < 50 && m[i]; i++) { unsigned char *p = m[i]; if (p[0] != i || p[19999] != i) allok = 0; free (m[i]); }
    ok (allok, "the malloc blocks hold what was written");
    ok (flex_set_budge (0) == (da ? -1 : 0), "flex_set_budge (0): 0 in a slot, -1 with a dynamic area (UnixLib: nothing to register)"); }
  sprintf (buf, "size of b %d, of c %d", flex_size (&b), flex_size (&c)); info ("%s", buf);
  flex_free (&b); flex_free (&c);
  ok (b == NULL && c == NULL, "all freed");
}

static void test_sflib (void)
{
  char buf[100], *p;
  enum msgs_status st;

  ok (string_copy (buf, "hello world", 6) == buf && !strcmp (buf, "hello"), "string_copy (len 6 leaves 'hello')");
  string_toupper (strcpy (buf, "MiXed 1")); ok (!strcmp (buf, "MIXED 1"), "string_toupper");
  string_tolower (strcpy (buf, "MiXed 1")); ok (!strcmp (buf, "mixed 1"), "string_tolower");
  ok (string_nocase_strcmp ("Hello", "hELLO") == 0 && string_nocase_strcmp ("a", "b") < 0, "string_nocase_strcmp");
  { char *s = "The Quick Brown Fox", *f = string_nocase_strstr (s, "quick"), *n = string_nocase_strstr ("abc", "xyz"); ok (f == s + 4 && n != NULL && *n == 0, "string_nocase_strstr (found: the position; not found: the end of the string, not NULL)"); }
  ok (string_wildcard_compare ("*.txt", "a.TXT", 1) && !string_wildcard_compare ("*.txt", "a.TXT", 0) && string_wildcard_compare ("a#c", "abc", 0), "string_wildcard_compare (case and wildcards)");
  strcpy (buf, "  padded  "); ok (!strcmp (string_strip_surrounding_whitespace (buf), "padded"), "string_strip_surrounding_whitespace");
  strcpy (buf, "ADFS::Disc.$.Dir.File"); ok (!strcmp (string_find_leafname (buf), "File"), "string_find_leafname");
  strcpy (buf, "Dir.file/txt"); ok (!strcmp (string_find_extension (buf), "txt"), "string_find_extension ('txt' in the RISC OS name 'file/txt')");
  strcpy (buf, "v1.23"); ok (string_convert_version_number (buf) > 0, "string_convert_version_number");
  ok (string_printf (buf, sizeof buf, "%d-%s", 42, "x") > 0 && !strcmp (buf, "42-x"), "string_printf");

  stack_push (11); stack_push (22); stack_push (33);
  ok (stack_pull () == 33 && stack_pop () == 22 && stack_pull () == 22 && stack_pull () == 11, "stack_push / stack_pull / stack_pop (pop only looks at the top)");

  ok (heap_initialise (), "heap_initialise (a heap in a flex block)");
  p = heap_alloc (100); ok (p != NULL, "heap_alloc 100");
  if (p) { memset (p, 'x', 100); char *q = heap_alloc (3000); ok (q != NULL, "heap_alloc 3000");
    if (q) { memset (q, 'y', 3000); q = heap_extend (q, 9000); ok (q && heap_size (q) >= 9000, "heap_extend 3000 -> 9000, heap_size"); ok (q && q[0] == 'y' && q[2999] == 'y', "heap_extend keeps the contents"); heap_free (q); }
    ok (p[0] == 'x' && p[99] == 'x', "the first block is intact"); heap_free (p); }
  { char *s = heap_strdup ("duplicated"); ok (s && !strcmp (s, "duplicated"), "heap_strdup"); heap_free (s); }

  { FILE *f = fopen ("<FlexSF$Dir>.Messages", "r"); if (f) fclose (f); ok (f != NULL, "the Messages file of the pack exists"); }
  ok (msgs_initialise ("<FlexSF$Dir>.Messages"), "msgs_initialise");
  p = msgs_lookup ("Hello", buf, sizeof buf); ok (p && !strcmp (p, "Hello from the message file"), "msgs_lookup (Hello)");
  p = msgs_param_lookup ("Param", buf, sizeof buf, "one", "two", NULL, NULL); ok (p && !strcmp (p, "A one and a two"), "msgs_param_lookup (%0 and %1)");
  st = msgs_lookup_result ("NoSuchToken", buf, sizeof buf); ok (st == MSGS_STATUS_ERROR, "msgs_lookup_result of a token that is not there: MSGS_STATUS_ERROR");
  msgs_terminate ();

  config_opt_init ("flag", 1); config_int_init ("count", 7);
  ok (config_opt_read ("flag") == 1 && config_int_read ("count") == 7, "config_opt_init / config_int_init, read");
  config_opt_set ("flag", 0); config_int_set ("count", 99);
  ok (config_opt_read ("flag") == 0 && config_int_read ("count") == 99, "config_opt_set / config_int_set");

  info ("general_mode_width %d, general_mode_height %d (OS units)", general_mode_width (), general_mode_height ());
  ok (general_mode_width () > 0 && general_mode_height () > 0, "general_mode_width / height");
}

int main (void)
{
  char mode[20] = "slot";
  int da, buflen = 0;
  wimp_t task;
  os_error *e;
  char text[200];
  char *dir;

  { const char *m = getenv ("FlexSF$Mode"); if (m) { strncpy (mode, m, sizeof mode - 1); mode[sizeof mode - 1] = 0; } }
  da = !strcmp (mode, "da");
  out = fopen ("<FlexSF$Dir>.Results", "w");
  if (!out) return 1;
  e = xwimp_initialise (wimp_VERSION_RO38, "FlexSF", NULL, NULL, &task);
  ok (e == NULL, "wimp_initialise");
  (void) buflen; (void) dir;
  fprintf (out, "mode %s\n", mode);
  ok (flex_init ("FlexSF", 0, da ? 8 * 1024 * 1024 : 0) >= 0, "flex_init");
  info ("flex_init: %s", da ? "dynamic area" : "slot");
  test_flex (da);
  test_sflib ();
  fprintf (out, "FlexSF (%s): %d checks, %d FAILED\n", mode, checks, failed);
  fclose (out);
  sprintf (text, "FlexSF (%s): %d checks, %d FAILED. See the file Results in the application folder.", mode, checks, failed);
  { os_error err; err.errnum = 0; strcpy (err.errmess, text); xwimp_report_error (&err, wimp_ERROR_BOX_OK_ICON | wimp_ERROR_BOX_NO_PROMPT, "FlexSF", NULL); }
  xwimp_close_down (task);
  return failed != 0;
}
