/* t_pipeleft.c - a program that leaves a pipe () (and a file) open when it ends must not break the next program: run it several times in a row (the job ul_pp).  RISC OS does not close files at exit;
   _exit () of the SDK does. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main (int argc, char **argv)
{
  int p[2], fd;
  if (pipe (p)) { printf ("FAILED: pipe: errno %d\n", errno); return 1; }
  fd = open ("left_open_file", O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd < 0) { printf ("FAILED: open: errno %d\n", errno); return 1; }
  if (write (fd, "x", 1) != 1) { printf ("FAILED: write\n"); return 1; }
  printf ("PASSED: pipe %d %d, file %d left open\n", p[0], p[1], fd);
  if (argc > 1) _exit (0);
  return 0;				/* nothing closed */
}
