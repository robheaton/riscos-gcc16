/* t_dup2std.c - dup2 onto the standard descriptors: a file becomes stdout or stdin (gawk and many programs do this: remap a closed stream to /dev/null). */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf (stderr, "FAIL %s (line %d, errno %d)\n", #c, __LINE__, errno); } else fprintf (stderr, "ok   %s\n", #c); } while (0)

int main (void)
{
  int fd, n;
  char buf[32];
  FILE *f;
  /* stdout to a file */
  fd = open ("d2_out", O_WRONLY | O_CREAT | O_TRUNC, 0644);
  CHECK (fd >= 0);
  CHECK (dup2 (fd, 1) == 1);
  CHECK (close (fd) == 0);
  CHECK (write (1, "hello\n", 6) == 6);
  CHECK (close (1) == 0);
  f = fopen ("d2_out", "r");
  CHECK (f && fgets (buf, sizeof buf, f) && !strcmp (buf, "hello\n"));
  if (f) fclose (f);
  /* stdin from a file */
  f = fopen ("d2_in", "w"); fputs ("abc\n", f); fclose (f);
  fd = open ("d2_in", O_RDONLY);
  CHECK (fd >= 0 && dup2 (fd, 0) == 0);
  CHECK (close (fd) == 0);
  CHECK (read (0, buf, sizeof buf) == 4 && !memcmp (buf, "abc\n", 4));
  CHECK (read (0, buf, sizeof buf) == 0);
  /* remap a stream to /dev/null (what gawk does when it closes stdin) */
  fd = open ("/dev/null", O_RDWR);
  CHECK (fd >= 0 && dup2 (fd, 0) == 0);
  CHECK (close (fd) == 0);
  n = (int) read (0, buf, sizeof buf);
  CHECK (n == 0);
  CHECK (isatty (0) == 0);
  CHECK (close (0) == 0);
  remove ("d2_out"); remove ("d2_in");
  fprintf (stderr, fails ? "FAILED: %d failures\n" : "PASSED: %d failures\n", fails);
  return fails != 0;
}
