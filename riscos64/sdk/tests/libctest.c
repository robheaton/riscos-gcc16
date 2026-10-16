/* libctest - a few checks of picolibc on RISC OS 64 through the system calls of crt/riscos_syscalls.c: stdio on files, string and number conversions, qsort, maths, setjmp, malloc. */
#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf ("FAIL %s (line %d)\n", #c, __LINE__); } else printf ("PASS %s\n", #c); } while (0)

static int cmp (const void *a, const void *b) { int x = *(const int *) a, y = *(const int *) b; return (x > y) - (x < y); }
static jmp_buf jb;

int main (void)
{
  char buf[64], line[80];
  int v[8] = { 5, 3, 9, 1, 7, 2, 8, 4 }, i;
  FILE *f;
  void *p[16];
  printf ("RISC OS 64 test libctest (picolibc)\n");

  /* stdio on a file: OS_Find, OS_GBPB, OS_Args */
  f = fopen ("libcfile", "w");
  CHECK (f != NULL);
  if (f) { fprintf (f, "line one\nline two %d\n", 42); fwrite ("0123456789", 1, 10, f); CHECK (fclose (f) == 0); }
  f = fopen ("libcfile", "r");
  CHECK (f != NULL);
  if (f)
    {
      CHECK (fgets (line, sizeof line, f) && strcmp (line, "line one\n") == 0);
      CHECK (fgets (line, sizeof line, f) && strcmp (line, "line two 42\n") == 0);
      CHECK (fread (buf, 1, 10, f) == 10 && memcmp (buf, "0123456789", 10) == 0);
      CHECK (fread (buf, 1, 1, f) == 0 && feof (f));
      CHECK (fseek (f, 5, SEEK_SET) == 0 && ftell (f) == 5 && fgetc (f) == 'o');
      CHECK (fseek (f, 0, SEEK_END) == 0 && ftell (f) == 31);
      fclose (f);
    }
  CHECK (fopen ("no.such.file.here", "r") == NULL);

  /* strings and numbers */
  CHECK (snprintf (buf, sizeof buf, "%08.3f|%-5d|%+ld|%#x|%s", 3.14159, 42, 7L, 255, "ok") == 25 && strcmp (buf, "0003.142|42   |+7|0xff|ok") == 0);
  CHECK (strtol ("  -1234abc", NULL, 10) == -1234 && strtoul ("ff", NULL, 16) == 255 && strtoll ("9223372036854775807", NULL, 10) == 9223372036854775807LL);
  CHECK (strtod ("1.5e3", NULL) == 1500.0 && atof ("0.25") == 0.25 && atoi ("77") == 77);
  CHECK (sscanf ("12 abc 3.5", "%d %s %lf", &i, buf, &(double){0}) == 3 && i == 12);
  CHECK (strlen ("hello") == 5 && strchr ("hello", 'l') && strstr ("hello", "llo") && strncmp ("abc", "abd", 2) == 0);

  /* qsort, malloc */
  qsort (v, 8, sizeof v[0], cmp);
  CHECK (v[0] == 1 && v[3] == 4 && v[7] == 9);
  for (i = 0; i < 16; i++) { p[i] = malloc (100 + i * 37); if (p[i]) memset (p[i], i, 100); }
  for (i = 0; i < 16; i++) CHECK (p[i] != NULL && ((unsigned char *) p[i])[99] == i);
  for (i = 0; i < 16; i++) free (p[i]);
  CHECK (realloc (NULL, 10) != NULL);

  /* maths: hardware floating point */
  CHECK (fabs (sqrt (2.0) - 1.4142135623730951) < 1e-15 && fabs (sin (1.0) - 0.8414709848078965) < 1e-15 && fabs (pow (2.0, 0.5) - sqrt (2.0)) < 1e-15);
  CHECK (floor (-1.5) == -2.0 && ceil (1.2) == 2.0 && fmod (7.5, 2.0) == 1.5 && fabs (exp (1.0) - 2.718281828459045) < 1e-15 && fabs (log (10.0) - 2.302585092994046) < 1e-15);

  /* setjmp, longjmp */
  i = setjmp (jb);
  if (i == 0) longjmp (jb, 7);
  CHECK (i == 7);

  printf ("SUMMARY libctest: %d checks, %d failed\n", checks, fails);
  return fails ? 1 : 0;
}
