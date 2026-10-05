/* host-sulfile-test.c -- test identify () of sulfile.c on the build host with the REAL module files of the build tree (sul-build):  each must give its own status and description; a file with one byte changed, an empty
   buffer and a non-module must give 3; the header text of an unknown module is shown.
   build + run:  gcc -std=gnu11 -O1 -Wall -Wextra -DSULFILE_HOST host-sulfile-test.c -o /tmp/host-sulfile-test && /tmp/host-sulfile-test ~/gccsdk-next/sul-build */
#define SULFILE_HOST 1
#include "sulfile.c"

static unsigned char *slurp (const char *dir, const char *name, size_t *n)
{
  char p[512]; snprintf (p, sizeof p, "%s/%s", dir, name);
  FILE *f = fopen (p, "rb"); if (!f) { perror (p); exit (2); }
  fseek (f, 0, SEEK_END); *n = (size_t) ftell (f); fseek (f, 0, SEEK_SET);
  unsigned char *b = malloc (*n + 1); if (fread (b, 1, *n, f) != *n) exit (2); fclose (f); return b;
}
static int bad;
static void expect (int ok, const char *what) { if (!ok) { printf ("FAIL: %s\n", what); bad++; } }

int main (int argc, char **argv)
{
  const char *dir = argc > 1 ? argv[1] : "sul-build";
  struct { const char *file; int code; const char *contains; } t[] = {
    { "sul-ref.bin", 0, "STOCK" }, { "SharedULib-116fix3,ffa", 2, "THE FIXED" }, { "SharedULib-116fix2,ffa", 3, "1.16-vforkfix2" }, { "SharedULib-116fix1,ffa", 3, "1.16-vforkfix1" },
    { "SharedULib-116orig,ffa", 3, "1.16-orig" }, { "SharedULib-116fix3t,ffa", 3, "vforkfix3t" }, { "SharedULib-116fix2t,ffa", 3, "vforkfix2t" },
  };
  for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
    size_t n; unsigned char *b = slurp (dir, t[i].file, &n); unsigned h; char text[400];
    int c = identify (b, n, &h, text, sizeof text);
    printf ("%-26s %5zu bytes  FNV %08x  status %d  %s\n", t[i].file, n, h, c, text);
    expect (c == t[i].code && strstr (text, t[i].contains) != NULL, t[i].file);
    b[n / 2] ^= 1; c = identify (b, n, &h, text, sizeof text);                   /* one bit flipped in the middle: no longer a known file */
    expect (c == 3 && strstr (text, "NOT a file") != NULL, "a flipped bit is not recognised");
    free (b);
  }
  size_t n; unsigned char *b = slurp (dir, "sul-ref.bin", &n); unsigned h; char text[400];
  expect (identify (b, n - 4, &h, text, sizeof text) == 3, "a truncated file");
  expect (identify (b, 0, &h, text, sizeof text) == 3, "an empty buffer");
  b[16] = 0xFF; b[17] = 0xFF;                                                      /* a broken title offset: no header text, no crash */
  expect (identify (b, n, &h, text, sizeof text) == 3 && strstr (text, "does not look like a module") != NULL, "a broken header");
  unsigned char junk[64]; memset (junk, 'x', sizeof junk);
  expect (identify (junk, sizeof junk, &h, text, sizeof text) == 3, "junk");
  /* an unknown file that IS a module: the stock one with a changed byte at its end shows the header text */
  free (b); b = slurp (dir, "sul-ref.bin", &n); b[n - 1] ^= 0x55;
  int c = identify (b, n, &h, text, sizeof text);
  printf("unknown module: status %d  %s\n", c, text);
  expect (c == 3 && strstr (text, "SharedUnixLibrary") != NULL && strstr (text, "1.16") != NULL, "header text of an unknown module");
  free (b);
  if (bad) { printf ("%d checks FAILED\n", bad); return 1; }
  printf ("ok: sulfile identify (): every build of ours has its own status, anything else is 3, no crash on odd input\n");
  return 0;
}
