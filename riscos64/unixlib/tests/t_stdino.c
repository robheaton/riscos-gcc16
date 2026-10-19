/* t_stdino.c - standard input and output redirected to two different files are two different files for fstat (tools such as cat refuse "input file is output file" when the device and inode are the same);
   run with Sh$Stdin and Sh$Stdout set to two files (the test sets them for a child: t_stdino child) or by the shell:  t_stdino < a > b.  Redirected output is a regular file. */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf (stderr, "FAIL %s (line %d)\n", #c, __LINE__); } else fprintf (stderr, "ok   %s\n", #c); } while (0)
int main (int argc, char **argv)
{
  struct stat in, out;
  (void) argc; (void) argv;
  CHECK (fstat (0, &in) == 0);
  CHECK (fstat (1, &out) == 0);
  fprintf (stderr, "stdin: mode %o ino %lu size %ld   stdout: mode %o ino %lu\n", (unsigned) in.st_mode, (unsigned long) in.st_ino, (long) in.st_size, (unsigned) out.st_mode, (unsigned long) out.st_ino);
  CHECK (S_ISREG (in.st_mode) && S_ISREG (out.st_mode));
  CHECK (in.st_ino != 0 && out.st_ino != 0);
  CHECK (in.st_ino != out.st_ino);
  printf ("x\n");
  fflush (stdout);
  fprintf (stderr, fails ? "FAILED: %d\n" : "PASSED\n", fails);
  return fails != 0;
}
