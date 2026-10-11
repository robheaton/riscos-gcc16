/* rtest.c - a Wimp task that tests RUfl (NetSurf's Unicode font library) built for GCC 16 and UnixLib.  It writes <RuflTest$Dir>.Results (one line per check: ok / FAIL / info) and then opens a window that shows
   text in several fonts and scripts, painted by rufl_paint: look at it (it should show the lines of text in different typefaces, with the accents, Greek and Cyrillic readable) and close it to end the task. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <strings.h>
#include <stdint.h>
#include <stdbool.h>
#include "rufl.h"
#include <oslib/os.h>
#include <oslib/wimp.h>

static FILE *out;
static int checks, failed;
static void ok (int cond, const char *what) { checks++; if (!cond) failed++; fprintf (out, "%s %s\n", cond ? "ok  " : "FAIL", what); fflush (out); }
static void info (const char *fmt, ...) { va_list ap; va_start (ap, fmt); fputs ("info ", out); vfprintf (out, fmt, ap); fputc ('\n', out); va_end (ap); fflush (out); }

static const char *family;           /* the family used for the checks */
static int nmove, nline, ncubic;
static int mv (os_coord *to, void *u) { (void) to; (void) u; nmove++; return 0; }
static int ln (os_coord *to, void *u) { (void) to; (void) u; nline++; return 0; }
static int cu (os_coord *c1, os_coord *c2, os_coord *to, void *u) { (void) c1; (void) c2; (void) to; (void) u; ncubic++; return 0; }
static int cb_calls, cb_chars;
static void cb (void *ctx, const char *name, unsigned int size, const uint8_t *s8, const uint32_t *s32, unsigned int n, int x, int y)
{ (void) ctx; (void) name; (void) size; (void) s8; (void) s32; (void) x; (void) y; cb_calls++; cb_chars += (int) n; }

static int width (const char *fam, unsigned size, const char *s)
{
  int w = -1;
  if (rufl_width (fam, rufl_WEIGHT_400, size, s, strlen (s), &w) != rufl_OK) return -1;
  return w;
}

static void run_checks (void)
{
  rufl_code c;
  size_t i, off = 99;
  int w1, w2, w3, ax = -1;
  os_box bb; int32_t xk, yk, it, asc, desc, xh, caph; int8_t up; uint8_t ut;
  int32_t xb, yb, gw, gh, xa, ya;

  c = rufl_init ();
  ok (c == rufl_OK, "rufl_init (scans the fonts of the Font Manager)");
  if (c != rufl_OK) { info ("rufl_init gave %d%s%s", (int) c, rufl_fm_error ? ": " : "", rufl_fm_error ? rufl_fm_error->errmess : ""); return; }
  ok (rufl_family_list_entries > 0, "the family list is not empty");
  info ("%u font families, the first ones:", (unsigned) rufl_family_list_entries);
  for (i = 0; i < rufl_family_list_entries && i < 8; i++) info ("  %s", rufl_family_list[i]);
  family = NULL;
  for (i = 0; i < rufl_family_list_entries; i++) if (!strcasecmp (rufl_family_list[i], "Homerton")) family = rufl_family_list[i];
  if (!family && rufl_family_list_entries) family = rufl_family_list[0];
  info ("the family used for the checks: %s", family ? family : "(none)");
  if (!family) return;

  w1 = width (family, 160, "Hello"); w2 = width (family, 160, "Hello, world"); w3 = width (family, 320, "Hello");
  info ("widths in OS units at 10 pt: 'Hello' %d, 'Hello, world' %d; at 20 pt: 'Hello' %d", w1, w2, w3);
  ok (w1 > 0, "rufl_width of 'Hello' is positive");
  ok (w2 > w1, "a longer text is wider");
  ok (w3 > w1 * 18 / 10 && w3 < w1 * 22 / 10, "twice the size is about twice the width");
  ok (width (family, 160, "") == 0, "rufl_width of an empty text is 0");
  ok (width (family, 160, "\xe2\x82\xac") > 0, "the euro sign (UTF-8 E2 82 AC) has a width");
  c = rufl_width (family, rufl_WEIGHT_400, 160, "\xce\xb1\xce\xb2\xce\xb3", 6, &w3);
  ok (c == rufl_OK, "Greek alpha beta gamma: rufl_OK (a font with the characters is found, or the substitution makes one up)");
  info ("Greek width %d", w3);
  c = rufl_width (family, rufl_WEIGHT_400, 160, "\xd0\xb6\xd0\xb7\xd0\xb8", 6, &w3);
  ok (c == rufl_OK, "Cyrillic zhe ze i: rufl_OK");
  info ("Cyrillic width %d", w3);
  c = rufl_width (family, rufl_WEIGHT_700, 160, "Hello", 5, &w3);
  ok (c == rufl_OK && w3 > 0, "bold: rufl_OK and a width");
  c = rufl_width (family, (rufl_style) (rufl_WEIGHT_400 | rufl_SLANTED), 160, "Hello", 5, &w3);
  ok (c == rufl_OK && w3 > 0, "slanted: rufl_OK and a width");

  c = rufl_split (family, rufl_WEIGHT_400, 160, "Hello, world", 12, w1 + 1, &off, &ax);
  ok (c == rufl_OK && off >= 5 && off < 12 && ax <= w1 + 1, "rufl_split of 'Hello, world' into the width of 'Hello' + 1: the prefix is 5 or more characters and fits");
  info ("rufl_split: %u characters, %d wide", (unsigned) off, ax);
  c = rufl_x_to_offset (family, rufl_WEIGHT_400, 160, "Hello", 5, w1 / 2, &off, &ax);
  ok (c == rufl_OK && off <= 5 && ax <= w1, "rufl_x_to_offset in the middle of 'Hello'");
  info ("rufl_x_to_offset: offset %u, x %d", (unsigned) off, ax);

  c = rufl_font_metrics (family, rufl_WEIGHT_400, &bb, &xk, &yk, &it, &asc, &desc, &xh, &caph, &up, &ut);
  ok (c == rufl_OK && asc > 0 && caph > 0 && xh > 0, "rufl_font_metrics: ascent, cap height and x-height are positive");
  info ("metrics: bbox %d %d %d %d, ascent %d, descent %d, x-height %d, cap height %d", bb.x0, bb.y0, bb.x1, bb.y1, (int) asc, (int) desc, (int) xh, (int) caph);
  c = rufl_glyph_metrics (family, rufl_WEIGHT_400, 160, "A", 1, &xb, &yb, &gw, &gh, &xa, &ya);
  ok (c == rufl_OK && xa > 0 && gw > 0 && gh > 0, "rufl_glyph_metrics of 'A': advance, width and height are positive");
  info ("glyph A: advance %d, size %d x %d", (int) xa, (int) gw, (int) gh);
  c = rufl_font_bbox (family, rufl_WEIGHT_400, 160, &bb);
  ok (c == rufl_OK && bb.x1 > bb.x0 && bb.y1 > bb.y0, "rufl_font_bbox is not empty");
  { struct rufl_decomp_funcs f = { mv, ln, cu };
    c = rufl_decompose_glyph (family, rufl_WEIGHT_400, 160, "A", 1, &f, NULL);
    ok (c == rufl_OK && nmove > 0 && (nline + ncubic) > 0, "rufl_decompose_glyph of 'A' gives a path (moves and lines or curves)");
    info ("path of A: %d moves, %d lines, %d curves", nmove, nline, ncubic); }
  c = rufl_paint_callback (family, rufl_WEIGHT_400, 160, "Hi", 2, 0, 0, cb, NULL);
  ok (c == rufl_OK && cb_calls >= 1 && cb_chars == 2, "rufl_paint_callback of 'Hi' calls back for 2 characters in total");
  c = rufl_width ("NoSuchFontFamilyAtAll", rufl_WEIGHT_400, 160, "Hello", 5, &w3);
  info ("rufl_width of a family that does not exist gave code %d (0 = OK: substituted), width %d", (int) c, w3);
  rufl_invalidate_cache ();
  ok (width (family, 160, "Hello") == w1, "after rufl_invalidate_cache the width is the same");
}

static wimp_w win;
static const struct { const char *fam; unsigned size; const char *text; } lines[] = {
  { "Homerton", 320, "RUfl on GCC 16: Hello, world" },
  { "Homerton", 240, "Accents: \xc3\x80\xc3\x89\xc3\x8e\xc3\x95\xc3\x9c \xc3\xa0\xc3\xa9\xc3\xae\xc3\xb5\xc3\xbc  \xc3\x9f \xc3\xa7 \xc3\xb1" },
  { "Trinity", 240, "Euro \xe2\x82\xac, pound \xc2\xa3, dash \xe2\x80\x94, ellipsis \xe2\x80\xa6" },
  { "Corpus", 240, "Greek: \xce\xb1\xce\xb2\xce\xb3\xce\xb4 \xce\xa9\xce\xa3" },
  { "Homerton", 240, "Cyrillic: \xd0\xb6\xd0\xb7\xd0\xb8\xd0\xba \xd0\x96\xd0\x94" },
  { "Trinity", 160, "The quick brown fox jumps over the lazy dog" },
};

static void redraw (wimp_draw *d)
{
  osbool more = wimp_redraw_window (d);
  while (more)
    {
      int x = d->box.x0 - d->xscroll + 24, y = d->box.y1 - d->yscroll - 60;
      for (unsigned i = 0; i < sizeof lines / sizeof lines[0]; i++)
        {
          rufl_paint (lines[i].fam, rufl_WEIGHT_400, lines[i].size, lines[i].text, strlen (lines[i].text), x, y, 0);
          y -= lines[i].size * 3 / 2 / 16 * 2 + 20;
        }
      more = wimp_get_rectangle (d);
    }
}

int main (void)
{
  wimp_t task; os_error *e;
  char text[200];
  wimp_window w;
  wimp_block b;
  int quit = 0;

  out = fopen ("<RuflTest$Dir>.Results", "w");
  if (!out) return 1;
  e = xwimp_initialise (wimp_VERSION_RO38, "RuflTest", NULL, NULL, &task);
  ok (e == NULL, "wimp_initialise");
  run_checks ();
  fprintf (out, "RuflTest: %d checks, %d FAILED\n", checks, failed);
  fclose (out);
  sprintf (text, "RuflTest: %d checks, %d FAILED. See the file Results in the application folder. Click OK to open the window with the text.", checks, failed);
  { os_error err; err.errnum = 0; strcpy (err.errmess, text); xwimp_report_error (&err, wimp_ERROR_BOX_OK_ICON | wimp_ERROR_BOX_NO_PROMPT, "RuflTest", NULL); }

  memset (&w, 0, sizeof w);
  w.visible.x0 = 100; w.visible.y0 = 300; w.visible.x1 = 1300; w.visible.y1 = 900;
  w.xscroll = 0; w.yscroll = 0; w.next = wimp_TOP;
  w.flags = wimp_WINDOW_MOVEABLE | wimp_WINDOW_NEW_FORMAT | wimp_WINDOW_CLOSE_ICON | wimp_WINDOW_TITLE_ICON | wimp_WINDOW_BACK_ICON | wimp_WINDOW_TOGGLE_ICON | wimp_WINDOW_SIZE_ICON;
  w.title_fg = wimp_COLOUR_BLACK; w.title_bg = wimp_COLOUR_LIGHT_GREY; w.work_fg = wimp_COLOUR_BLACK; w.work_bg = wimp_COLOUR_WHITE;
  w.scroll_outer = wimp_COLOUR_MID_LIGHT_GREY; w.scroll_inner = wimp_COLOUR_VERY_LIGHT_GREY; w.highlight_bg = wimp_COLOUR_CREAM;
  w.extent.x0 = 0; w.extent.y0 = -600; w.extent.x1 = 1200; w.extent.y1 = 0;
  w.title_flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED | wimp_ICON_HCENTRED | wimp_ICON_FILLED;
  w.work_flags = (wimp_BUTTON_NEVER << wimp_ICON_BUTTON_TYPE_SHIFT);
  w.sprite_area = (osspriteop_area *) 1; w.xmin = 200; w.ymin = 200;
  w.title_data.indirected_text.text = "RUfl on GCC 16"; w.title_data.indirected_text.validation = (char *) -1; w.title_data.indirected_text.size = 15;
  w.icon_count = 0;
  e = xwimp_create_window (&w, &win);
  if (!e) { wimp_open o; memset (&o, 0, sizeof o); o.w = win; o.visible = w.visible; o.next = wimp_TOP; xwimp_open_window (&o); }
  while (!quit && !e)
    {
      wimp_event_no ev = wimp_poll (wimp_MASK_NULL | wimp_MASK_POLLWORD, &b, NULL);
      switch (ev)
        {
        case wimp_REDRAW_WINDOW_REQUEST: redraw (&b.redraw); break;
        case wimp_OPEN_WINDOW_REQUEST: wimp_open_window (&b.open); break;
        case wimp_CLOSE_WINDOW_REQUEST: quit = 1; break;
        case wimp_USER_MESSAGE: case wimp_USER_MESSAGE_RECORDED: if (b.message.action == message_QUIT) quit = 1; break;
        default: break;
        }
    }
  rufl_quit ();
  xwimp_close_down (task);
  return failed != 0;
}
