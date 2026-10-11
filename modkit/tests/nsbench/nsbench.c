/* nsbench.c - a benchmark for the parts of NetSurf that take the time, to compare builds of the libraries with different compiler flags (docs/NETSURF.md).  A program for a Task window (no Wimp): usage
     nsbench DIR          DIR holds thtml, bcss, pjpg and ipng (the names have no dot: UnixLib would swap a suffix into a directory name; made by modkit/tests/nsbench/make-data.py)
   Each test runs until at least 4 seconds have passed and prints the milliseconds for one pass; the numbers are only comparable between builds on the same machine. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <setjmp.h>
#include <dom/dom.h>
#include <dom/bindings/hubbub/parser.h>
#include <libcss/libcss.h>
#include <jpeglib.h>
#include <png.h>
#include <zlib.h>

static double now (void) { return (double) clock () / CLOCKS_PER_SEC; }
static unsigned char *slurp (const char *dir, const char *name, size_t *len)
{
  char p[512]; FILE *f; unsigned char *b;
  /* a RISC OS directory name (with : or $ in it) takes a dot, any other a slash */
  snprintf (p, sizeof p, "%s%c%s", dir, (strchr (dir, ':') || strchr (dir, '$')) ? '.' : '/', name);
  f = fopen (p, "rb");
  if (!f) { fprintf (stderr, "cannot open %s\n", p); exit (1); }
  fseek (f, 0, SEEK_END); *len = (size_t) ftell (f); fseek (f, 0, SEEK_SET);
  b = malloc (*len + 1); if (!b || fread (b, 1, *len, f) != *len) { fprintf (stderr, "cannot read %s\n", p); exit (1); }
  b[*len] = 0; fclose (f);
  return b;
}
static volatile unsigned sink;
#define RUN(name, extra, body) do { double t0 = now (), t1; int n = 0; do { body; n++; t1 = now (); } while (t1 - t0 < 4.0); \
  printf ("%-14s %9.2f ms per pass  (%d passes)  %s\n", name, (t1 - t0) * 1000.0 / n, n, extra); fflush (stdout); } while (0)

static dom_hubbub_error no_script (void *ctx, struct dom_node *node) { (void) ctx; (void) node; return DOM_HUBBUB_OK; }
static void no_msg (uint32_t severity, void *ctx, const char *msg, ...) { (void) severity; (void) ctx; (void) msg; }
static css_error resolve_url (void *pw, const char *base, lwc_string *rel, lwc_string **abs) { (void) pw; (void) base; *abs = lwc_string_ref (rel); return CSS_OK; }

static unsigned count_nodes (dom_node *n)
{
  unsigned c = 1; dom_node *ch, *next;
  dom_node_get_first_child (n, &ch);
  while (ch) { c += count_nodes (ch); dom_node_get_next_sibling (ch, &next); dom_node_unref (ch); ch = next; }
  return c;
}
static unsigned html_pass (const unsigned char *d, size_t len, int verify)
{
  unsigned nodes = 0;
  dom_hubbub_parser_params p; dom_hubbub_parser *parser; dom_document *doc;
  memset (&p, 0, sizeof p);
  p.enc = "UTF-8"; p.fix_enc = true; p.enable_script = false; p.script = no_script; p.msg = no_msg; p.ctx = NULL;
  if (dom_hubbub_parser_create (&p, &parser, &doc) != DOM_HUBBUB_OK) { puts ("parser create failed"); exit (1); }
  dom_hubbub_parser_parse_chunk (parser, d, len);
  dom_hubbub_parser_completed (parser);
  if (verify) nodes = count_nodes ((dom_node *) doc);
  dom_hubbub_parser_destroy (parser);
  dom_node_unref (doc);
  return nodes;
}
static size_t css_pass (const unsigned char *d, size_t len, int verify)
{
  css_stylesheet_params p; css_stylesheet *sheet; size_t size = 0;
  memset (&p, 0, sizeof p);
  p.params_version = CSS_STYLESHEET_PARAMS_VERSION_1; p.level = CSS_LEVEL_21; p.charset = "UTF-8"; p.url = "bench"; p.resolve = resolve_url;
  if (css_stylesheet_create (&p, &sheet) != CSS_OK) { puts ("css create failed"); exit (1); }
  css_stylesheet_append_data (sheet, d, len);
  css_stylesheet_data_done (sheet);
  if (verify) css_stylesheet_size (sheet, &size);
  css_stylesheet_destroy (sheet);
  return size;
}
static unsigned jpeg_pass (const unsigned char *d, size_t len, int verify)
{
  struct jpeg_decompress_struct c; struct jpeg_error_mgr e; unsigned char *row; unsigned sum = 0, i;
  c.err = jpeg_std_error (&e);
  jpeg_create_decompress (&c);
  jpeg_mem_src (&c, d, len);
  jpeg_read_header (&c, TRUE);
  c.out_color_space = JCS_RGB;
  jpeg_start_decompress (&c);
  row = malloc (c.output_width * c.output_components);
  while (c.output_scanline < c.output_height) { unsigned char *r = row; jpeg_read_scanlines (&c, &r, 1); sum += row[0]; if (verify) for (i = 0; i < c.output_width * c.output_components; i++) sum = sum * 31 + row[i]; }
  free (row);
  jpeg_finish_decompress (&c); jpeg_destroy_decompress (&c);
  return sum;
}
struct memrd { const unsigned char *d; size_t len, pos; };
static void png_read_mem (png_structp p, png_bytep out, size_t n) { struct memrd *m = png_get_io_ptr (p); if (m->pos + n > m->len) png_error (p, "eof"); memcpy (out, m->d + m->pos, n); m->pos += n; }
static unsigned png_pass (const unsigned char *d, size_t len, int verify)
{
  png_structp p = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  png_infop info = png_create_info_struct (p);
  struct memrd m = { d, len, 0 }; unsigned sum = 0; png_uint_32 h, i, k; png_bytep row;
  if (setjmp (png_jmpbuf (p))) { puts ("png error"); exit (1); }
  png_set_read_fn (p, &m, png_read_mem);
  png_read_info (p, info);
  png_set_expand (p); png_set_strip_16 (p); png_set_gray_to_rgb (p); png_set_filler (p, 0xff, PNG_FILLER_AFTER);
  png_read_update_info (p, info);
  h = png_get_image_height (p, info);
  row = malloc (png_get_rowbytes (p, info));
  for (i = 0; i < h; i++) { png_read_row (p, row, NULL); sum += row[0]; if (verify) for (k = 0; k < png_get_rowbytes (p, info); k++) sum = sum * 31 + row[k]; }
  free (row); png_destroy_read_struct (&p, &info, NULL);
  return sum;
}
static unsigned inflate_pass (const unsigned char *z, size_t zlen, unsigned char *out, size_t outlen)
{
  uLongf n = (uLongf) outlen;
  uncompress (out, &n, z, (uLong) zlen);
  return out[0];
}
static volatile unsigned vseed = 1;       /* the loops depend on it, so that an optimiser cannot compute them once and reuse the result */
static unsigned idiv_loop (unsigned s0) { unsigned s = s0; for (unsigned i = 1; i < 4000000; i++) s += (i * 2654435761u) / (i % 7 + 3) + (i * 40503u) % 1009; return s; }
static double fp_loop (unsigned s0) { double s = s0, x = 1.0; for (int i = 1; i < 3000000; i++) { x = x * 1.0000001 + 0.5 / i; s += x / (i + 1.0) + (double) (i & 15) * 0.25; } return s; }

int main (int argc, char **argv)
{
  size_t hl, cl, jl, pl; unsigned char *html, *css, *jpg, *png, *raw, *z; uLongf zl; size_t rawlen = 8u << 20; unsigned i;
  const char *dir = argc > 1 ? argv[1] : ".";
  html = slurp (dir, "thtml", &hl); css = slurp (dir, "bcss", &cl); jpg = slurp (dir, "pjpg", &jl); png = slurp (dir, "ipng", &pl);
  printf ("nsbench: html %u bytes, css %u bytes, jpeg %u bytes, png %u bytes\n", (unsigned) hl, (unsigned) cl, (unsigned) jl, (unsigned) pl);
  raw = malloc (rawlen); for (i = 0; i < rawlen; i++) raw[i] = (unsigned char) ((i * 31 + (i >> 7) * 17) ^ (i >> 11));
  zl = compressBound (rawlen); z = malloc (zl); compress2 (z, &zl, raw, (uLong) rawlen, 6);
  { size_t cs; unsigned nodes = html_pass (html, hl, 1);
    cs = css_pass (css, cl, 1);
    printf ("check: html nodes %u, css size %u, jpeg %08x, png %08x, inflate adler32 %08x\n", nodes, (unsigned) cs, jpeg_pass (jpg, jl, 1), png_pass (png, pl, 1), (unsigned) (inflate_pass (z, zl, raw, rawlen), adler32 (1, raw, (uInt) rawlen)));
    printf ("check: integer loop %u, floating point loop %.17g\n", idiv_loop (vseed), fp_loop (vseed)); fflush (stdout); }
  RUN ("html parse", "(hubbub + libdom)", html_pass (html, hl, 0));
  RUN ("css parse", "(libcss)", css_pass (css, cl, 0));
  RUN ("jpeg decode", "(libjpeg-turbo)", sink += jpeg_pass (jpg, jl, 0));
  RUN ("png decode", "(libpng + zlib)", sink += png_pass (png, pl, 0));
  RUN ("inflate 8 MB", "(zlib)", sink += inflate_pass (z, zl, raw, rawlen));
  RUN ("integer divide", "(4M iterations)", sink += idiv_loop (vseed));
  RUN ("floating point", "(3M iterations)", sink += (unsigned) fp_loop (vseed));
  printf ("nsbench: done\n");
  return 0;
}
