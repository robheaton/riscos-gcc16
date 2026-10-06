/* host-test.c -- test the parts of sulfile.c that do not need RISC OS, on the build host: identify () with the REAL module files that build-sul.sh makes, and the three small functions that
   find the real name of a file that a path variable finds (split_pathvar, next_element, join_name).
   build + run:  gcc -std=gnu11 -O1 -Wall -Wextra -DSULFILE_HOST host-test.c -o /tmp/sulfile-host-test && /tmp/sulfile-host-test ~/gccsdk-next/sul-build */
#define SULFILE_HOST 1
#include "sulfile.c"

static unsigned char *slurp (const char *dir, const char *name, size_t *n)
{
  char p[512]; snprintf (p, sizeof p, "%s/%s", dir, name);
  FILE *f = fopen (p, "rb"); if (!f) { perror (p); exit (2); }
  fseek (f, 0, SEEK_END); *n = (size_t) ftell (f); fseek (f, 0, SEEK_SET);
  unsigned char *b = malloc (*n + 1); if (fread (b, 1, *n, f) != *n) exit (2); fclose (f); return b;
}
static int bad, checks;
static void expect (int ok, const char *what) { checks++; if (!ok) { printf ("FAIL: %s\n", what); bad++; } }

int main (int argc, char **argv)
{
  const char *dir = argc > 1 ? argv[1] : "sul-build";
  /* the files of build-sul.sh: the stock module (the unpatched source) and the fixed one; the test builds of the debugging work must NOT be taken for either */
  struct { const char *file; int code; const char *contains; } t[] = {
    { "sul-ref.bin", 0, "STOCK" }, { "SharedULib-116fix3,ffa", 2, "FIXED" }, { "SharedULib-116fix2,ffa", 3, "1.16-vforkfix2" }, { "SharedULib-116fix1,ffa", 3, "1.16-vforkfix1" },
    { "SharedULib-116orig,ffa", 3, "1.16-orig" }, { "SharedULib-116fix3t,ffa", 3, "vforkfix3t" },
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
  free (b);

  /* the path variable names */
  char var[64]; const char *rest;
  expect (split_pathvar ("System:Modules.SharedULib", var, sizeof var, &rest) && !strcmp (var, "System") && !strcmp (rest, "Modules.SharedULib"), "System:Modules.SharedULib");
  expect (split_pathvar ("Boot:Resources.x", var, sizeof var, &rest) && !strcmp (var, "Boot") && !strcmp (rest, "Resources.x"), "Boot:Resources.x");
  expect (!split_pathvar ("ADFS::4.$.Foo", var, sizeof var, &rest), "ADFS::4.$.Foo is a disc name, not a path variable");
  expect (!split_pathvar ("ADFS:$.Foo", var, sizeof var, &rest), "ADFS:$.Foo is a file system, not a path variable");
  expect (!split_pathvar ("NVMe::NVMe.$.!Boot.Resources.!System.310.Modules.SharedULib", var, sizeof var, &rest), "a full name");
  expect (!split_pathvar ("$.Foo", var, sizeof var, &rest), "$.Foo");
  expect (!split_pathvar ("Foo", var, sizeof var, &rest), "a leaf name");
  expect (!split_pathvar ("System:", var, sizeof var, &rest), "System: with nothing after it");
  expect (!split_pathvar (":x", var, sizeof var, &rest), ":x");
  expect (!split_pathvar ("Sys<Dir>:x", var, sizeof var, &rest) || strcmp (var, "Sys") != 0, "no '<' tricks");

  /* the directories of System$Path as they look on the Pi, and the names made from them */
  const char *list = "Sys:500.,Sys:400.,Sys:370.,Sys:360.,Sys:350.,Sys:310.,NVMe::NVMe.$.!Boot.Resources.!System.";
  const char *want[] = { "Sys:500.", "Sys:400.", "Sys:370.", "Sys:360.", "Sys:350.", "Sys:310.", "NVMe::NVMe.$.!Boot.Resources.!System." };
  char el[512]; int k = 0;
  for (const char *p = list; (p = next_element (p, el, sizeof el)) != NULL; k++) expect (k < 7 && !strcmp (el, want[k]), "next_element: one of the seven");
  expect (k == 7, "next_element: seven elements");
  char out[128];
  expect (join_name ("Sys:310.", "Modules.SharedULib", out, sizeof out) && !strcmp (out, "Sys:310.Modules.SharedULib"), "join with a dot at the end");
  expect (join_name ("Boot:", "x", out, sizeof out) && !strcmp (out, "Boot:x"), "join with a colon at the end");
  expect (join_name ("ADFS::4.$.Mods", "x", out, sizeof out) && !strcmp (out, "ADFS::4.$.Mods.x"), "join without a separator at the end");
  expect (!join_name ("", "x", out, sizeof out), "an empty element");
  expect (!join_name ("A.", "0123456789", out, 8), "a name that does not fit");
  k = 0; for (const char *p = "a.,,b."; (p = next_element (p, el, sizeof el)) != NULL; k++) ;
  expect (k == 3, "an empty element in the middle is returned (and skipped by join_name)");
  k = 0; for (const char *p = ""; (p = next_element (p, el, sizeof el)) != NULL; k++) ;
  expect (k == 0, "an empty list");
  printf ("%d checks, %d failed\n", checks, bad);
  return bad != 0;
}
