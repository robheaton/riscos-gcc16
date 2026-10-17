/* t_glob.c - glob () over a few files made here (needs -munixlib: directories are read with opendir/readdir). */
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fails;
#define CHECK(c) do { if (c) printf ("ok   %s\n", #c); else { printf ("FAIL %s\n", #c); fails++; } } while (0)
static void touch (const char *n) { FILE *f = fopen (n, "w"); if (f) { fputs ("x\n", f); fclose (f); } }
static int names (glob_t *g, const char *want)		/* the matches joined with spaces equal want */
{
  char all[512] = "";
  int i;
  for (i = 0; i < g->gl_pathc; i++) { if (i) strcat (all, " "); strcat (all, g->gl_pathv[i]); }
  if (strcmp (all, want)) printf ("     got \"%s\", want \"%s\"\n", all, want);
  return strcmp (all, want) == 0;
}

int main (void)
{
  glob_t g;
  mkdir ("gdir", 0777);
  mkdir ("gdir/sub", 0777);
  touch ("gdir/a1.c"); touch ("gdir/a2.c"); touch ("gdir/b.h"); touch ("gdir/.hid"); touch ("gdir/sub/x.c");

  CHECK (glob ("gdir/*.c", 0, 0, &g) == 0 && names (&g, "gdir/a1.c gdir/a2.c"));
  globfree (&g);
  CHECK (glob ("gdir/a?.c", 0, 0, &g) == 0 && g.gl_pathc == 2);
  globfree (&g);
  CHECK (glob ("gdir/*", 0, 0, &g) == 0 && names (&g, "gdir/a1.c gdir/a2.c gdir/b.h gdir/sub"));	/* no .hid */
  globfree (&g);
  CHECK (glob ("gdir/*", GLOB_MARK, 0, &g) == 0 && strcmp (g.gl_pathv[3], "gdir/sub/") == 0);
  globfree (&g);
  CHECK (glob ("gdir/*/x.c", 0, 0, &g) == 0 && names (&g, "gdir/sub/x.c"));
  globfree (&g);
  CHECK (glob ("gdir/.h*", 0, 0, &g) == 0 && names (&g, "gdir/.hid"));
  globfree (&g);
  CHECK (glob ("gdir/{a1,b}.*", GLOB_BRACE, 0, &g) == 0 && names (&g, "gdir/a1.c gdir/b.h"));
  globfree (&g);
  CHECK (glob ("gdir/nothing*", 0, 0, &g) == GLOB_NOMATCH);
  CHECK (glob ("gdir/nothing*", GLOB_NOCHECK, 0, &g) == 0 && names (&g, "gdir/nothing*"));
  globfree (&g);
  CHECK (glob ("gdir/a1.c", 0, 0, &g) == 0 && names (&g, "gdir/a1.c"));
  CHECK (glob ("gdir/b.*", GLOB_APPEND, 0, &g) == 0 && names (&g, "gdir/a1.c gdir/b.h"));
  globfree (&g);

  remove ("gdir/a1.c"); remove ("gdir/a2.c"); remove ("gdir/b.h"); remove ("gdir/.hid"); remove ("gdir/sub/x.c");
  rmdir ("gdir/sub"); rmdir ("gdir");
  printf ("%s: %d failures\n", fails ? "FAILED" : "PASSED", fails);
  return fails != 0;
}
