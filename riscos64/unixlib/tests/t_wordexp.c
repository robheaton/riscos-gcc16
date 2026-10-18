/* t_wordexp.c - wordexp: quotes, variables, tilde, command substitution, IFS splitting, globbing, errors, append/offsets. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wordexp.h>

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf ("FAIL %s (line %d)\n", #c, __LINE__); } else printf ("ok   %s\n", #c); } while (0)

/* expand s; true when it gives exactly the words in list (separated by '|') */
static int is (const char *s, const char *list, int flags)
{
  wordexp_t w;
  char got[512] = "";
  size_t i;
  int r = wordexp (s, &w, flags);
  if (r) { printf ("   (%s -> error %d)\n", s, r); return 0; }
  for (i = 0; i < w.we_wordc; i++) { if (i) strcat (got, "|"); strcat (got, w.we_wordv[i]); }
  wordfree (&w);
  if (strcmp (got, list)) { printf ("   (%s -> [%s], wanted [%s])\n", s, got, list); return 0; }
  return 1;
}
static int err (const char *s, int flags) { wordexp_t w; int r = wordexp (s, &w, flags); if (!r) wordfree (&w); return r; }

int main (void)
{
  wordexp_t w;
  setenv ("WX", "alpha beta", 1);
  setenv ("ONE", "x", 1);
  setenv ("HOME", "/home/me", 1);
  unsetenv ("NOPE");
  CHECK (is ("a b  c", "a|b|c", 0));
  CHECK (is ("  lead trail  ", "lead|trail", 0));
  CHECK (is ("", "", 0));
  CHECK (is ("'a  b' c", "a  b|c", 0));
  CHECK (is ("\"a  b\" c", "a  b|c", 0));
  CHECK (is ("a\\ b", "a b", 0));
  CHECK (is ("x\"y z\"w", "xy zw", 0));
  CHECK (is ("''", "", 0));
  CHECK (is ("$WX", "alpha|beta", 0));
  CHECK (is ("\"$WX\"", "alpha beta", 0));
  CHECK (is ("${ONE}y", "xy", 0));
  CHECK (is ("$ONEy", "", 0));
  CHECK (is ("${#WX}", "10", 0));
  CHECK (is ("${NOPE:-dflt}", "dflt", 0));
  CHECK (is ("${ONE:-dflt}", "x", 0));
  CHECK (is ("${ONE:+yes}", "yes", 0));
  CHECK (is ("${NOPE:+yes}", "", 0));
  CHECK (is ("\"\\$ONE\"", "$ONE", 0));
  CHECK (is ("cost $", "cost|$", 0));
  CHECK (is ("~", "/home/me", 0));
  CHECK (is ("~/dir", "/home/me/dir", 0));
  CHECK (is ("'~'", "~", 0));
  CHECK (is ("a~", "a~", 0));
  CHECK (is ("$(echo one two)", "one|two", 0));
  CHECK (is ("\"$(echo one two)\"", "one two", 0));
  CHECK (is ("`echo bq`", "bq", 0));
  CHECK (err ("$(echo hi)", WRDE_NOCMD) == WRDE_CMDSUB);
  CHECK (err ("`echo hi`", WRDE_NOCMD) == WRDE_CMDSUB);
  CHECK (err ("a | b", 0) == WRDE_BADCHAR);
  CHECK (err ("a;b", 0) == WRDE_BADCHAR);
  CHECK (err ("a > b", 0) == WRDE_BADCHAR);
  CHECK (err ("'open", 0) == WRDE_SYNTAX);
  CHECK (err ("\"open", 0) == WRDE_SYNTAX);
  CHECK (err ("${open", 0) == WRDE_SYNTAX);
  CHECK (err ("$NOPE", WRDE_UNDEF) == WRDE_BADVAL);
  CHECK (err ("$ONE", WRDE_UNDEF) == 0);
  /* globbing in a scratch directory */
  mkdir ("wxd", 0777);
  { FILE *f; f = fopen ("wxd/aa1", "w"); if (f) fclose (f); f = fopen ("wxd/aa2", "w"); if (f) fclose (f); f = fopen ("wxd/b*c", "w"); if (f) fclose (f); }
  CHECK (is ("wxd/aa*", "wxd/aa1|wxd/aa2", 0));
  CHECK (is ("wxd/aa?", "wxd/aa1|wxd/aa2", 0));
  CHECK (is ("wxd/zz*", "wxd/zz*", 0));
  CHECK (is ("'wxd/aa*'", "wxd/aa*", 0));
  CHECK (is ("wxd/aa\\*", "wxd/aa*", 0));
  /* append and offsets */
  CHECK (wordexp ("one two", &w, 0) == 0 && w.we_wordc == 2);
  CHECK (wordexp ("three", &w, WRDE_APPEND) == 0 && w.we_wordc == 3 && !strcmp (w.we_wordv[0], "one") && !strcmp (w.we_wordv[2], "three") && !w.we_wordv[3]);
  wordfree (&w);
  w.we_offs = 2;
  CHECK (wordexp ("p q", &w, WRDE_DOOFFS) == 0 && w.we_wordc == 2 && !w.we_wordv[0] && !w.we_wordv[1] && !strcmp (w.we_wordv[2], "p") && !strcmp (w.we_wordv[3], "q") && !w.we_wordv[4]);
  wordfree (&w);
  remove ("wxd/aa1"); remove ("wxd/aa2"); remove ("wxd/b*c"); rmdir ("wxd");
  printf (fails ? "FAILED: %d failures\n" : "PASSED: %d failures\n", fails);
  return fails != 0;
}
